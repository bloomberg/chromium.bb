// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Windows Vista uses the Native Wifi (WLAN) API for accessing WiFi cards. See
// http://msdn.microsoft.com/en-us/library/ms705945(VS.85).aspx. Windows XP
// Service Pack 3 (and Windows XP Service Pack 2, if upgraded with a hot fix)
// also support a limited version of the WLAN API. See
// http://msdn.microsoft.com/en-us/library/bb204766.aspx. The WLAN API uses
// wlanapi.h, which is not part of the SDK used by Gears, so is replicated
// locally using data from the MSDN.
//
// Windows XP from Service Pack 2 onwards supports the Wireless Zero
// Configuration (WZC) programming interface. See
// http://msdn.microsoft.com/en-us/library/ms706587(VS.85).aspx. 
//
// The MSDN recommends that one use the WLAN API where available, and WZC
// otherwise.
//
// However, it seems that WZC fails for some wireless cards. Also, WLAN seems
// not to work on XP SP3. So we use WLAN on Vista, and use NDIS directly
// otherwise.

// TODO(cprince): remove platform-specific #ifdef guards when OS-specific
// sources (e.g. WIN32_CPPSRCS) are implemented
#if defined(WIN32) && !defined(OS_WINCE)

#include "gears/geolocation/wifi_data_provider_win32.h"

#include <windows.h>
#include <ntddndis.h>  // For IOCTL_NDIS_QUERY_GLOBAL_STATS
#include "gears/base/common/string_utils.h"
#include "gears/base/common/vista_utils.h"
#include "gears/geolocation/wifi_data_provider_common.h"
#include "gears/geolocation/wifi_data_provider_windows_common.h"

// Taken from ndis.h for WinCE.
#define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)

// The limits on the size of the buffer used for the OID query.
static const int kInitialBufferSize = 2 << 12;  // Good for about 50 APs.
static const int kMaximumBufferSize = 2 << 20;  // 2MB

// Length for generic string buffers passed to Win32 APIs.
static const int kStringLength = 512;

// The time periods, in milliseconds, between successive polls of the wifi data.
extern const int kDefaultPollingInterval = 10000;  // 10s
extern const int kNoChangePollingInterval = 120000;  // 2 mins
extern const int kTwoNoChangePollingInterval = 600000;  // 10 mins

// Local functions

// Extracts data for an access point and converts to Gears format.
static bool GetNetworkData(const WLAN_BSS_ENTRY &bss_entry,
                           AccessPointData *access_point_data);
bool UndefineDosDevice(const std::string16 &device_name);
bool DefineDosDeviceIfNotExists(const std::string16 &device_name);
HANDLE GetFileHandle(const std::string16 &device_name);
// Makes the OID query and returns a Win32 error code.
int PerformQuery(HANDLE adapter_handle,
                 BYTE *buffer,
                 DWORD buffer_size,
                 DWORD *bytes_out);
bool ResizeBuffer(int requested_size, BYTE **buffer);
// Gets the system directory and appends a trailing slash if not already
// present.
bool GetSystemDirectory(std::string16 *path);

// static
template<>
WifiDataProviderImplBase *WifiDataProvider::DefaultFactoryFunction() {
  return new Win32WifiDataProvider();
}


Win32WifiDataProvider::Win32WifiDataProvider()
    : oid_buffer_size_(kInitialBufferSize),
      is_first_scan_complete_(false) {
  // Start the polling thread.
  Start();
}

Win32WifiDataProvider::~Win32WifiDataProvider() {
  stop_event_.Signal();
  Join();
}

bool Win32WifiDataProvider::GetData(WifiData *data) {
  assert(data);
  MutexLock lock(&data_mutex_);
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

// Thread implementation
void Win32WifiDataProvider::Run() {
  // We use an absolute path to load the DLL to avoid DLL preloading attacks.
  HINSTANCE library = NULL;
  std::string16 system_directory;
  if (GetSystemDirectory(&system_directory)) {
    assert(!system_directory.empty());
    std::string16 dll_path = system_directory + L"wlanapi.dll";
    library = LoadLibraryEx(dll_path.c_str(),
                            NULL,
                            LOAD_WITH_ALTERED_SEARCH_PATH);
  }

  // Use the WLAN interface if we're on Vista and if it's available. Otherwise,
  // use NDIS.
  typedef bool (Win32WifiDataProvider::*GetAccessPointDataFunction)(
      WifiData::AccessPointDataSet *data);
  GetAccessPointDataFunction get_access_point_data_function = NULL;
  if (VistaUtils::IsRunningOnVista() && library) {
    GetWLANFunctions(library);
    get_access_point_data_function =
        &Win32WifiDataProvider::GetAccessPointDataWLAN;
  } else {
    // We assume the list of interfaces doesn't change while Gears is running.
    if (!GetInterfacesNDIS()) {
      is_first_scan_complete_ = true;
      return;
    }
    get_access_point_data_function =
        &Win32WifiDataProvider::GetAccessPointDataNDIS;
  }
  assert(get_access_point_data_function);

  int polling_interval = kDefaultPollingInterval;
  // Regularly get the access point data.
  do {
    WifiData new_data;
    if ((this->*get_access_point_data_function)(&new_data.access_point_data)) {
      bool update_available;
      data_mutex_.Lock();
      update_available = wifi_data_.DiffersSignificantly(new_data);
      wifi_data_ = new_data;
      data_mutex_.Unlock();
      polling_interval =
          UpdatePollingInterval(polling_interval, update_available);
      if (update_available) {
        is_first_scan_complete_ = true;
        NotifyListeners();
      }
    }
  } while (!stop_event_.WaitWithTimeout(polling_interval));

  FreeLibrary(library);
}

// WLAN functions

void Win32WifiDataProvider::GetWLANFunctions(HINSTANCE wlan_library) {
  assert(wlan_library);
  WlanOpenHandle_function_ = reinterpret_cast<WlanOpenHandleFunction>(
      GetProcAddress(wlan_library, "WlanOpenHandle"));
  WlanEnumInterfaces_function_ = reinterpret_cast<WlanEnumInterfacesFunction>(
      GetProcAddress(wlan_library, "WlanEnumInterfaces"));
  WlanGetNetworkBssList_function_ =
      reinterpret_cast<WlanGetNetworkBssListFunction>(
      GetProcAddress(wlan_library, "WlanGetNetworkBssList"));
  WlanFreeMemory_function_ = reinterpret_cast<WlanFreeMemoryFunction>(
      GetProcAddress(wlan_library, "WlanFreeMemory"));
  WlanCloseHandle_function_ = reinterpret_cast<WlanCloseHandleFunction>(
      GetProcAddress(wlan_library, "WlanCloseHandle"));
  assert(WlanOpenHandle_function_ &&
         WlanEnumInterfaces_function_ &&
         WlanGetNetworkBssList_function_ &&
         WlanFreeMemory_function_ &&
         WlanCloseHandle_function_);
}

bool Win32WifiDataProvider::GetAccessPointDataWLAN(
    WifiData::AccessPointDataSet *data) {
  assert(data);

  // Get the handle to the WLAN API.
  DWORD negotiated_version;
  HANDLE wlan_handle = NULL;
  // We could be executing on either Windows XP or Windows Vista, so use the
  // lower version of the client WLAN API. It seems that the negotiated version
  // is the Vista version irrespective of what we pass!
  static const int kXpWlanClientVersion = 1;
  if ((*WlanOpenHandle_function_)(kXpWlanClientVersion,
                                  NULL,
                                  &negotiated_version,
                                  &wlan_handle) != ERROR_SUCCESS) {
    return false;
  }
  assert(wlan_handle);

  // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
  WLAN_INTERFACE_INFO_LIST *interface_list = NULL;
  if ((*WlanEnumInterfaces_function_)(wlan_handle, NULL, &interface_list) !=
      ERROR_SUCCESS) {
    return false;
  }
  assert(interface_list);

  // Go through the list of interfaces and get the data for each.
  for (int i = 0; i < static_cast<int>(interface_list->dwNumberOfItems); ++i) {
    GetInterfaceDataWLAN(wlan_handle,
                         interface_list->InterfaceInfo[i].InterfaceGuid,
                         data);
  }

  // Free interface_list.
  (*WlanFreeMemory_function_)(interface_list);

  // Close the handle.
  if ((*WlanCloseHandle_function_)(wlan_handle, NULL) != ERROR_SUCCESS) {
    return false;
  }

  return true;
}

// Appends the data for a single interface to the data vector. Returns the
// number of access points found, or -1 on error.
int Win32WifiDataProvider::GetInterfaceDataWLAN(
    const HANDLE wlan_handle,
    const GUID &interface_id,
    WifiData::AccessPointDataSet *data) {
  assert(data);
  // WlanGetNetworkBssList allocates bss_list.
  WLAN_BSS_LIST *bss_list;
  if ((*WlanGetNetworkBssList_function_)(wlan_handle,
                                         &interface_id,
                                         NULL,   // Use all SSIDs.
                                         DOT11_BSS_TYPE_UNUSED,
                                         false,  // bSecurityEnabled - unused
                                         NULL,   // reserved
                                         &bss_list) != ERROR_SUCCESS) {
    return -1;
  }

  int found = 0;
  for (int i = 0; i < static_cast<int>(bss_list->dwNumberOfItems); ++i) {
    AccessPointData access_point_data;
    if (GetNetworkData(bss_list->wlanBssEntries[i], &access_point_data)) {
      ++found;
      data->insert(access_point_data);
    }
  }

  (*WlanFreeMemory_function_)(bss_list);

  return found;
}

// NDIS functions

bool Win32WifiDataProvider::GetInterfacesNDIS() {
  HKEY network_cards_key = NULL;
  if (RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards",
      0,
      KEY_READ,
      &network_cards_key) != ERROR_SUCCESS) {
    return false;
  }
  assert(network_cards_key);

  for (int i = 0; ; ++i) {
    TCHAR name[kStringLength];
    DWORD name_size = kStringLength;
    FILETIME time;
    if (RegEnumKeyEx(network_cards_key,
                     i,
                     name,
                     &name_size,
                     NULL,
                     NULL,
                     NULL,
                     &time) != ERROR_SUCCESS) {
      break;
    }
    HKEY hardware_key = NULL;
    if (RegOpenKeyEx(network_cards_key, name, 0, KEY_READ, &hardware_key) !=
        ERROR_SUCCESS) {
      break;
    }
    assert(hardware_key);

    TCHAR service_name[kStringLength];
    DWORD service_name_size = kStringLength;
    DWORD type = 0;
    if (RegQueryValueEx(hardware_key,
                        L"ServiceName",
                        NULL,
                        &type,
                        reinterpret_cast<LPBYTE>(service_name),
                        &service_name_size) == ERROR_SUCCESS) {
      interface_service_names_.push_back(service_name);
    }
    RegCloseKey(hardware_key);
  }

  RegCloseKey(network_cards_key);
  return true;
}

bool Win32WifiDataProvider::GetAccessPointDataNDIS(
    WifiData::AccessPointDataSet *data) {
  assert(data);

  for (int i = 0; i < static_cast<int>(interface_service_names_.size()); ++i) {
    // First, check that we have a DOS device for this adapter.
    if (!DefineDosDeviceIfNotExists(interface_service_names_[i])) {
      continue;
    }

    // Get the handle to the device. This will fail if the named device is not
    // valid.
    HANDLE adapter_handle = GetFileHandle(interface_service_names_[i]);
    if (adapter_handle == INVALID_HANDLE_VALUE) {
      continue;
    }

    // Get the data.
    GetInterfaceDataNDIS(adapter_handle, data);

    // Clean up.
    CloseHandle(adapter_handle);
    UndefineDosDevice(interface_service_names_[i]);
  }

  return true;
}

bool Win32WifiDataProvider::GetInterfaceDataNDIS(
    HANDLE adapter_handle,
    WifiData::AccessPointDataSet *data) {
  assert(data);

  BYTE *buffer = reinterpret_cast<BYTE*>(malloc(oid_buffer_size_));
  if (buffer == NULL) {
    return false;
  }

  DWORD bytes_out;
  int result;

  while (true) {
    bytes_out = 0;
    result = PerformQuery(adapter_handle, buffer, oid_buffer_size_, &bytes_out);
    if (result == ERROR_GEN_FAILURE ||  // Returned by some Intel cards.
        result == ERROR_INSUFFICIENT_BUFFER ||
        result == ERROR_MORE_DATA ||
        result == NDIS_STATUS_INVALID_LENGTH ||
        result == NDIS_STATUS_BUFFER_TOO_SHORT) {
      // The buffer we supplied is too small, so increase it. bytes_out should
      // provide the required buffer size, but this is not always the case.
      if (bytes_out > static_cast<DWORD>(oid_buffer_size_)) {
        oid_buffer_size_ = bytes_out;
      } else {
        oid_buffer_size_ *= 2;
      }
      if (!ResizeBuffer(oid_buffer_size_, &buffer)) {
        oid_buffer_size_ = kInitialBufferSize;  // Reset for next time.
        return false;
      }
    } else {
      // The buffer is not too small.
      break;
    }
  }
  assert(buffer);

  if (result == ERROR_SUCCESS) {
    NDIS_802_11_BSSID_LIST* bssid_list =
        reinterpret_cast<NDIS_802_11_BSSID_LIST*>(buffer);
    GetDataFromBssIdList(*bssid_list, oid_buffer_size_, data);
  }

  free(buffer);
  return true;
}

// Local functions

static bool GetNetworkData(const WLAN_BSS_ENTRY &bss_entry,
                           AccessPointData *access_point_data) {
  // Currently we get only MAC address, signal strength and SSID.
  assert(access_point_data);
  access_point_data->mac_address = MacAddressAsString16(bss_entry.dot11Bssid);
  access_point_data->radio_signal_strength = bss_entry.lRssi;
  // bss_entry.dot11Ssid.ucSSID is not null-terminated.
  UTF8ToString16(reinterpret_cast<const char*>(bss_entry.dot11Ssid.ucSSID),
                 static_cast<ULONG>(bss_entry.dot11Ssid.uSSIDLength),
                 &access_point_data->ssid);
  // TODO(steveblock): Is it possible to get the following?
  //access_point_data->signal_to_noise
  //access_point_data->age
  //access_point_data->channel
  return true;
}

bool UndefineDosDevice(const std::string16 &device_name) {
  // We remove only the mapping we use, that is \Device\<device_name>.
  std::string16 target_path = L"\\Device\\" + device_name;
  return DefineDosDevice(
      DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
      device_name.c_str(),
      target_path.c_str()) == TRUE;
}

bool DefineDosDeviceIfNotExists(const std::string16 &device_name) {
  // We create a DOS device name for the device at \Device\<device_name>.
  std::string16 target_path = L"\\Device\\" + device_name;

  TCHAR target[kStringLength];
  if (QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0) {
    // Device already exists.
    return true;
  }

  if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (!DefineDosDevice(DDD_RAW_TARGET_PATH,
                       device_name.c_str(),
                       target_path.c_str())) {
    return false;
  }

  // Check that the device is really there.
  return QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0;
}

HANDLE GetFileHandle(const std::string16 &device_name) {
  // We access a device with DOS path \Device\<device_name> at
  // \\.\<device_name>.
  std::string16 formatted_device_name = L"\\\\.\\" + device_name;

  return CreateFile(formatted_device_name.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
                    0,  // security attributes
                    OPEN_EXISTING,
                    0,  // flags and attributes
                    INVALID_HANDLE_VALUE);
}

int PerformQuery(HANDLE adapter_handle,
                 BYTE *buffer,
                 DWORD buffer_size,
                 DWORD *bytes_out) {
  DWORD oid = OID_802_11_BSSID_LIST;
  if (!DeviceIoControl(adapter_handle,
                       IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &oid,
                       sizeof(oid),
                       buffer,
                       buffer_size,
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  return ERROR_SUCCESS;
}

bool ResizeBuffer(int requested_size, BYTE **buffer) {
  if (requested_size > kMaximumBufferSize) {
    free(*buffer);
    *buffer = NULL;
    return false;
  }

  BYTE *new_buffer = reinterpret_cast<BYTE*>(realloc(*buffer, requested_size));
  if (new_buffer == NULL) {
    free(*buffer);
    *buffer = NULL;
    return false;
  }

  *buffer = new_buffer;
  return true;
}

bool GetSystemDirectory(std::string16 *path) {
  assert(path);
  // Return value includes terminating NULL.
  int buffer_size = GetSystemDirectory(NULL, 0);
  if (buffer_size == 0) {
    return false;
  }
  char16 *buffer = new char16[buffer_size];

  // Return value excludes terminating NULL.
  int characters_written = GetSystemDirectory(buffer, buffer_size);
  if (characters_written == 0) {
    return false;
  }
  assert(characters_written == buffer_size - 1);

  path->assign(buffer);
  delete[] buffer;

  if (path->at(path->length() - 1) != L'\\') {
    path->append(L"\\");
  }
  return true;
}

#endif  // WIN32 && !OS_WINCE
