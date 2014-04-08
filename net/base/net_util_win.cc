// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <iphlpapi.h>
#include <wlanapi.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

struct WlanApi {
  typedef DWORD (WINAPI *WlanOpenHandleFunc)(
      DWORD, VOID*, DWORD*, HANDLE*);
  typedef DWORD (WINAPI *WlanEnumInterfacesFunc)(
      HANDLE, VOID*, WLAN_INTERFACE_INFO_LIST**);
  typedef DWORD (WINAPI *WlanQueryInterfaceFunc)(
      HANDLE, const GUID*, WLAN_INTF_OPCODE, VOID*, DWORD*, VOID**,
      WLAN_OPCODE_VALUE_TYPE*);
  typedef VOID (WINAPI *WlanFreeMemoryFunc)(VOID*);
  typedef DWORD (WINAPI *WlanCloseHandleFunc)(HANDLE, VOID*);

  WlanApi() : initialized(false) {
    // Use an absolute path to load the DLL to avoid DLL preloading attacks.
    static const wchar_t* const kDLL = L"%WINDIR%\\system32\\wlanapi.dll";
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStrings(kDLL, path, arraysize(path));
    module = ::LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module)
      return;

    open_handle_func = reinterpret_cast<WlanOpenHandleFunc>(
        ::GetProcAddress(module, "WlanOpenHandle"));
    enum_interfaces_func = reinterpret_cast<WlanEnumInterfacesFunc>(
        ::GetProcAddress(module, "WlanEnumInterfaces"));
    query_interface_func = reinterpret_cast<WlanQueryInterfaceFunc>(
        ::GetProcAddress(module, "WlanQueryInterface"));
    free_memory_func = reinterpret_cast<WlanFreeMemoryFunc>(
        ::GetProcAddress(module, "WlanFreeMemory"));
    close_handle_func = reinterpret_cast<WlanCloseHandleFunc>(
        ::GetProcAddress(module, "WlanCloseHandle"));
    initialized = open_handle_func && enum_interfaces_func &&
                  query_interface_func && free_memory_func &&
                  close_handle_func;
  }

  template <typename T>
  DWORD OpenHandle(DWORD client_version, DWORD* cur_version, T* handle) const {
    HANDLE temp_handle;
    DWORD result = open_handle_func(client_version, NULL, cur_version,
                                    &temp_handle);
    if (result != ERROR_SUCCESS)
      return result;
    handle->Set(temp_handle);
    return ERROR_SUCCESS;
  }

  HMODULE module;
  WlanOpenHandleFunc open_handle_func;
  WlanEnumInterfacesFunc enum_interfaces_func;
  WlanQueryInterfaceFunc query_interface_func;
  WlanFreeMemoryFunc free_memory_func;
  WlanCloseHandleFunc close_handle_func;
  bool initialized;
};

}  // namespace

namespace net {

bool FileURLToFilePath(const GURL& url, base::FilePath* file_path) {
  *file_path = base::FilePath();
  std::wstring& file_path_str = const_cast<std::wstring&>(file_path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  std::string path;
  std::string host = url.host();
  if (host.empty()) {
    // URL contains no host, the path is the filename. In this case, the path
    // will probably be preceeded with a slash, as in "/C:/foo.txt", so we
    // trim out that here.
    path = url.path();
    size_t first_non_slash = path.find_first_not_of("/\\");
    if (first_non_slash != std::string::npos && first_non_slash > 0)
      path.erase(0, first_non_slash);
  } else {
    // URL contains a host: this means it's UNC. We keep the preceeding slash
    // on the path.
    path = "\\\\";
    path.append(host);
    path.append(url.path());
  }

  if (path.empty())
    return false;
  std::replace(path.begin(), path.end(), '/', '\\');

  // GURL stores strings as percent-encoded UTF-8, this will undo if possible.
  path = UnescapeURLComponent(path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  if (!IsStringUTF8(path)) {
    // Not UTF-8, assume encoding is native codepage and we're done. We know we
    // are giving the conversion function a nonempty string, and it may fail if
    // the given string is not in the current encoding and give us an empty
    // string back. We detect this and report failure.
    file_path_str = base::SysNativeMBToWide(path);
    return !file_path_str.empty();
  }
  file_path_str.assign(base::UTF8ToWide(path));

  // We used to try too hard and see if |path| made up entirely of
  // the 1st 256 characters in the Unicode was a zero-extended UTF-16.
  // If so, we converted it to 'Latin-1' and checked if the result was UTF-8.
  // If the check passed, we converted the result to UTF-8.
  // Otherwise, we treated the result as the native OS encoding.
  // However, that led to http://crbug.com/4619 and http://crbug.com/14153
  return true;
}

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  // GetAdaptersAddresses() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();
  bool is_xp = base::win::GetVersion() < base::win::VERSION_VISTA;
  ULONG len = 0;
  ULONG flags = is_xp ? GAA_FLAG_INCLUDE_PREFIX : 0;
  // First get number of networks.
  ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &len);
  if (result != ERROR_BUFFER_OVERFLOW) {
    // There are 0 networks.
    return true;
  }
  scoped_ptr<char[]> buf(new char[len]);
  IP_ADAPTER_ADDRESSES *adapters =
      reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.get());
  result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &len);
  if (result != NO_ERROR) {
    LOG(ERROR) << "GetAdaptersAddresses failed: " << result;
    return false;
  }

  // These two variables are used below when this method is asked to pick a
  // IPv6 address which has the shortest lifetime.
  ULONG ipv6_valid_lifetime = 0;
  scoped_ptr<NetworkInterface> ipv6_address;

  for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter != NULL;
       adapter = adapter->Next) {
    // Ignore the loopback device.
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
      continue;
    }

    if (adapter->OperStatus != IfOperStatusUp) {
      continue;
    }

    // Ignore any HOST side vmware adapters with a description like:
    // VMware Virtual Ethernet Adapter for VMnet1
    // but don't ignore any GUEST side adapters with a description like:
    // VMware Accelerated AMD PCNet Adapter #2
    if (policy == EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES &&
        strstr(adapter->AdapterName, "VMnet") != NULL) {
      continue;
    }

    for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
         address; address = address->Next) {
      int family = address->Address.lpSockaddr->sa_family;
      if (family == AF_INET || family == AF_INET6) {
        IPEndPoint endpoint;
        if (endpoint.FromSockAddr(address->Address.lpSockaddr,
                                  address->Address.iSockaddrLength)) {
          // XP has no OnLinkPrefixLength field.
          size_t net_prefix = is_xp ? 0 : address->OnLinkPrefixLength;
          if (is_xp) {
            // Prior to Windows Vista the FirstPrefix pointed to the list with
            // single prefix for each IP address assigned to the adapter.
            // Order of FirstPrefix does not match order of FirstUnicastAddress,
            // so we need to find corresponding prefix.
            for (IP_ADAPTER_PREFIX* prefix = adapter->FirstPrefix; prefix;
                 prefix = prefix->Next) {
              int prefix_family = prefix->Address.lpSockaddr->sa_family;
              IPEndPoint network_endpoint;
              if (prefix_family == family &&
                  network_endpoint.FromSockAddr(prefix->Address.lpSockaddr,
                      prefix->Address.iSockaddrLength) &&
                  IPNumberMatchesPrefix(endpoint.address(),
                                        network_endpoint.address(),
                                        prefix->PrefixLength)) {
                net_prefix = std::max<size_t>(net_prefix, prefix->PrefixLength);
              }
            }
          }
          uint32 index =
              (family == AF_INET) ? adapter->IfIndex : adapter->Ipv6IfIndex;
          // Pick one IPv6 address with least valid lifetime.
          // The reason we are checking |ValidLifeftime| as there is no other
          // way identifying the interface type. Usually (and most likely) temp
          // IPv6 will have a shorter ValidLifetime value then the permanent
          // interface.
          if (family == AF_INET6 &&
              (policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE)) {
            if (ipv6_valid_lifetime == 0 ||
                ipv6_valid_lifetime > address->ValidLifetime) {
              ipv6_valid_lifetime = address->ValidLifetime;
              ipv6_address.reset(new NetworkInterface(adapter->AdapterName,
                                 base::SysWideToNativeMB(adapter->FriendlyName),
                                 index, endpoint.address(), net_prefix));
              continue;
            }
          }
          networks->push_back(
              NetworkInterface(adapter->AdapterName,
                               base::SysWideToNativeMB(adapter->FriendlyName),
                               index, endpoint.address(), net_prefix));
        }
      }
    }
  }

  if (ipv6_address.get()) {
    networks->push_back(*(ipv6_address.get()));
  }
  return true;
}

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  static base::LazyInstance<WlanApi>::Leaky lazy_wlanapi =
      LAZY_INSTANCE_INITIALIZER;

  struct WlanApiHandleTraits {
    typedef HANDLE Handle;

    static bool CloseHandle(HANDLE handle) {
      return lazy_wlanapi.Get().close_handle_func(handle, NULL) ==
          ERROR_SUCCESS;
    }
    static bool IsHandleValid(HANDLE handle) {
      return base::win::HandleTraits::IsHandleValid(handle);
    }
    static HANDLE NullHandle() {
      return base::win::HandleTraits::NullHandle();
    }
  };

  typedef base::win::GenericScopedHandle<
      WlanApiHandleTraits,
      base::win::DummyVerifierTraits> WlanHandle;

  struct WlanApiDeleter {
    inline void operator()(void* ptr) const {
      lazy_wlanapi.Get().free_memory_func(ptr);
    }
  };

  const WlanApi& wlanapi = lazy_wlanapi.Get();
  if (!wlanapi.initialized)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;

  WlanHandle client;
  DWORD cur_version = 0;
  const DWORD kMaxClientVersion = 2;
  DWORD result = wlanapi.OpenHandle(kMaxClientVersion, &cur_version, &client);
  if (result != ERROR_SUCCESS)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;

  WLAN_INTERFACE_INFO_LIST* interface_list_ptr = NULL;
  result = wlanapi.enum_interfaces_func(client, NULL, &interface_list_ptr);
  if (result != ERROR_SUCCESS)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;
  scoped_ptr<WLAN_INTERFACE_INFO_LIST, WlanApiDeleter> interface_list(
      interface_list_ptr);

  // Assume at most one connected wifi interface.
  WLAN_INTERFACE_INFO* info = NULL;
  for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
    if (interface_list->InterfaceInfo[i].isState ==
        wlan_interface_state_connected) {
      info = &interface_list->InterfaceInfo[i];
      break;
    }
  }

  if (info == NULL)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;

  WLAN_CONNECTION_ATTRIBUTES* conn_info_ptr;
  DWORD conn_info_size = 0;
  WLAN_OPCODE_VALUE_TYPE op_code;
  result = wlanapi.query_interface_func(
      client, &info->InterfaceGuid, wlan_intf_opcode_current_connection, NULL,
      &conn_info_size, reinterpret_cast<VOID**>(&conn_info_ptr), &op_code);
  if (result != ERROR_SUCCESS)
    return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  scoped_ptr<WLAN_CONNECTION_ATTRIBUTES, WlanApiDeleter> conn_info(
      conn_info_ptr);

  switch (conn_info->wlanAssociationAttributes.dot11PhyType) {
    case dot11_phy_type_fhss:
      return WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case dot11_phy_type_dsss:
      return WIFI_PHY_LAYER_PROTOCOL_B;
    case dot11_phy_type_irbaseband:
      return WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case dot11_phy_type_ofdm:
      return WIFI_PHY_LAYER_PROTOCOL_A;
    case dot11_phy_type_hrdsss:
      return WIFI_PHY_LAYER_PROTOCOL_B;
    case dot11_phy_type_erp:
      return WIFI_PHY_LAYER_PROTOCOL_G;
    case dot11_phy_type_ht:
      return WIFI_PHY_LAYER_PROTOCOL_N;
    default:
      return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  }
}

}  // namespace net
