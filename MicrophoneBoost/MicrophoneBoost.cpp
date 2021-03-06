#include "stdafx.h"
//#include "objbase.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#define EXIT_ON_ERROR(hres)   if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)   if ((punk) != NULL)  { (punk)->Release(); (punk) = NULL; }

HRESULT getMicrophoneBoostVolumeLevel(IMMDevice *pEndptDev, IAudioVolumeLevel** ppVolumeLevel)
{
	HRESULT hr = S_OK;
	DataFlow flow;
	IDeviceTopology *pDeviceTopology = NULL;
	IConnector *pConnFrom = NULL;
	IConnector *pConnTo = NULL;
	IPart *pPartPrev = NULL;
	IPart *pPartNext = NULL;
	*ppVolumeLevel = NULL;
	wchar_t microphoneBoostName[] = L"Microphone Boost";//if your system language is English,the name is "microphone boost"
	if (pEndptDev == NULL)
	{
		EXIT_ON_ERROR(hr = E_POINTER)
	}

	// Get the endpoint device's IDeviceTopology interface.
	hr = pEndptDev->Activate(
		__uuidof(IDeviceTopology), CLSCTX_ALL, NULL,
		(void**)&pDeviceTopology);
	EXIT_ON_ERROR(hr)
		// The device topology for an endpoint device always
		// contains just one connector (connector number 0).
		hr = pDeviceTopology->GetConnector(0, &pConnFrom);
	SAFE_RELEASE(pDeviceTopology)
		EXIT_ON_ERROR(hr)
		// Make sure that this is a capture device.
		hr = pConnFrom->GetDataFlow(&flow);
	EXIT_ON_ERROR(hr)
		if (flow != Out)
		{
			// Error -- this is a rendering device.
			//EXIT_ON_ERROR(hr = AUDCLNT_E_WRONG_ENDPOINT_TYPE)
		}
	// Outer loop: Each iteration traverses the data path
	// through a device topology starting at the input
	// connector and ending at the output connector.
	while (TRUE)
	{
		BOOL bConnected;
		hr = pConnFrom->IsConnected(&bConnected);
		EXIT_ON_ERROR(hr)
			// Does this connector connect to another device?
			if (bConnected == FALSE)
			{
				// This is the end of the data path that
				// stretches from the endpoint device to the
				// system bus or external bus. Verify that
				// the connection type is Software_IO.
				ConnectorType  connType;
				hr = pConnFrom->GetType(&connType);
				EXIT_ON_ERROR(hr)
					if (connType == Software_IO)
					{
						break;  // finished
					}
				EXIT_ON_ERROR(hr = E_FAIL)
			}
		// Get the connector in the next device topology,
		// which lies on the other side of the connection.
		hr = pConnFrom->GetConnectedTo(&pConnTo);
		EXIT_ON_ERROR(hr)
			SAFE_RELEASE(pConnFrom)
			// Get the connector's IPart interface.
			hr = pConnTo->QueryInterface(
				__uuidof(IPart), (void**)&pPartPrev);
		EXIT_ON_ERROR(hr)
			SAFE_RELEASE(pConnTo)
			// Inner loop: Each iteration traverses one link in a
			// device topology and looks for input multiplexers.
			while (TRUE)
			{
				PartType parttype;
				IPartsList *pParts;
				// Follow downstream link to next part.
				hr = pPartPrev->EnumPartsOutgoing(&pParts);
				EXIT_ON_ERROR(hr)
					hr = pParts->GetPart(0, &pPartNext);
				pParts->Release();
				EXIT_ON_ERROR(hr)
					hr = pPartNext->GetPartType(&parttype);
				EXIT_ON_ERROR(hr)

					LPWSTR pName;
				if (SUCCEEDED(pPartNext->GetName(&pName)))
				{
					// Failure of the following call means only that
					// the part is not a boost (micrphone boost).
					if (wcscmp(microphoneBoostName, pName) == 0)
					{
						printf("Microphone Boost found \r\n");
						//get IAudioVolumeLevel to control volume
						hr = pPartNext->Activate(CLSCTX_ALL, __uuidof(IAudioVolumeLevel), (void**)ppVolumeLevel);
						goto Exit;
					}
					CoTaskMemFree(pName);
				}
				GUID subType;
				pPartNext->GetSubType(&subType);
				if (parttype == Connector)
				{
					// We've reached the output connector that
					// lies at the end of this device topology.
					hr = pPartNext->QueryInterface(
						__uuidof(IConnector),
						(void**)&pConnFrom);
					EXIT_ON_ERROR(hr)
						SAFE_RELEASE(pPartPrev)
						SAFE_RELEASE(pPartNext)
						break;
				}
				SAFE_RELEASE(pPartPrev)
					pPartPrev = pPartNext;
				pPartNext = NULL;
			}
	}
Exit:
	SAFE_RELEASE(pConnFrom)
		SAFE_RELEASE(pConnTo)
		SAFE_RELEASE(pPartPrev)
		SAFE_RELEASE(pPartNext)
		return hr;
}

extern "C"
{
	int __declspec(dllexport) setMicrophoneBoost(float CurrentDb)
	{
		HRESULT hr = NULL;
		CoInitialize(NULL);

		IMMDeviceEnumerator *deviceEnumerator = NULL;

		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
			__uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);

		IMMDevice *defaultDevice = NULL;
		IMMDeviceCollection *DeviceCollection = NULL;

		if (hr != S_OK)
		{
			return 0;
		}

		hr = deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &DeviceCollection);

		//hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &defaultDevice);
		deviceEnumerator->Release();

		deviceEnumerator = NULL;

		if (hr)
		{
			printf("EnumAudioEndpoints failed");
			return 0;
		}

		UINT deviceCount;
		hr = DeviceCollection->GetCount(&deviceCount);
		if (hr)
		{
			printf("IMMDeviceCollection->GetCount failed");
			return 0;
		}

		printf("Found %d devices\n", deviceCount);

		for (UINT i = 0; i < deviceCount; i++)
		{
			IMMDevice* device;
			hr = DeviceCollection->Item(i, &device);
			if (hr)
			{
				printf("IMMDeviceCollection->Item failed");
				return 0;
			}

			printf("  Device %d: \r\n", i);

			IAudioVolumeLevel* pIaudioVolumeLevel;
			getMicrophoneBoostVolumeLevel(device, &pIaudioVolumeLevel);

			device->Release();
			device = NULL;

			if (pIaudioVolumeLevel == NULL)
			{
				continue;
			}

			float fMinDb;
			float fMaxDb;
			float fStepDb;
			//float pfCurrentDb = 0.0f;
			pIaudioVolumeLevel->GetLevelRange(0, &fMinDb, &fMaxDb, &fStepDb);
			pIaudioVolumeLevel->SetLevel(0, CurrentDb, NULL);
		}

		return 0;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	printf("Set Microphone Boost to Max\r\n");
	//setMicrophoneBoost();
	//system("pause");
	return 0;
}

