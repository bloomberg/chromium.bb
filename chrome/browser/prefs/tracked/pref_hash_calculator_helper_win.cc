// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/pref_hash_calculator_helper.h"

// Implementation fully duplicated from
// chrome/browser/extensions/api/music_manager_private/device_id_win.cc (see
// pref_hash_calculator_helper_win.h for motivation).

// Note: The order of header includes is important, as we want both pre-Vista
// and post-Vista data structures to be defined, specifically
// PIP_ADAPTER_ADDRESSES and PMIB_IF_ROW2.
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_util.h"

namespace {

bool IsValidMacAddress(const void* bytes, size_t size) {
  const size_t MAC_LENGTH = 6;
  const size_t OUI_LENGTH = 3;
  struct InvalidMacEntry {
      size_t size;
      unsigned char address[MAC_LENGTH];
  };

  // VPN, virtualization, tethering, bluetooth, etc.
  static const InvalidMacEntry kInvalidAddresses[] = {
    // Empty address
    {MAC_LENGTH, {0, 0, 0, 0, 0, 0}},
    // VMware
    {OUI_LENGTH, {0x00, 0x50, 0x56}},
    {OUI_LENGTH, {0x00, 0x05, 0x69}},
    {OUI_LENGTH, {0x00, 0x0c, 0x29}},
    {OUI_LENGTH, {0x00, 0x1c, 0x14}},
    // VirtualBox
    {OUI_LENGTH, {0x08, 0x00, 0x27}},
    // PdaNet
    {MAC_LENGTH, {0x00, 0x26, 0x37, 0xbd, 0x39, 0x42}},
    // Cisco AnyConnect VPN
    {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x7a, 0x00}},
    // Marvell sometimes uses this as a dummy address
    {MAC_LENGTH, {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},
    // Apple uses this across machines for Bluetooth ethernet adapters.
    {MAC_LENGTH-1, {0x65, 0x90, 0x07, 0x42, 0xf1}},
    // Juniper uses this for their Virtual Adapter, the other 4 bytes are
    // reassigned at every boot. 00-ff-xx is not assigned to anyone.
    {2, {0x00, 0xff}},
    // T-Mobile Wireless Ethernet
    {MAC_LENGTH, {0x00, 0xa0, 0xc6, 0x00, 0x00, 0x00}},
    // Generic Bluetooth device
    {MAC_LENGTH, {0x00, 0x15, 0x83, 0x3d, 0x0a, 0x57}},
    // RAS Async Adapter
    {MAC_LENGTH, {0x20, 0x41, 0x53, 0x59, 0x4e, 0xff}},
    // Qualcomm USB ethernet adapter
    {MAC_LENGTH, {0x00, 0xa0, 0xc6, 0x00, 0x00, 0x00}},
    // Windows VPN
    {MAC_LENGTH, {0x00, 0x53, 0x45, 0x00, 0x00, 0x00}},
    // Bluetooth
    {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x08, 0x30}},
    {MAC_LENGTH, {0x00, 0x1b, 0x10, 0x00, 0x2a, 0xec}},
    {MAC_LENGTH, {0x00, 0x15, 0x83, 0x15, 0xa3, 0x10}},
    {MAC_LENGTH, {0x00, 0x15, 0x83, 0x07, 0xC6, 0x5A}},
    {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x02, 0x00}},
    {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x02, 0xdd}},
    // Ceton TV tuner
    {MAC_LENGTH, {0x00, 0x22, 0x2c, 0xff, 0xff, 0xff}},
    // Check Point VPN
    {MAC_LENGTH, {0x54, 0x55, 0x43, 0x44, 0x52, 0x09}},
    {MAC_LENGTH, {0x54, 0xEF, 0x14, 0x71, 0xE4, 0x0E}},
    {MAC_LENGTH, {0x54, 0xBA, 0xC6, 0xFF, 0x74, 0x10}},
    // Cisco VPN
    {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x7a, 0x00}},
    // Cisco VPN
    {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x78, 0x00}},
    // Intel USB cell modem
    {MAC_LENGTH, {0x00, 0x1e, 0x10, 0x1f, 0x00, 0x01}},
    // Microsoft tethering
    {MAC_LENGTH, {0x80, 0x00, 0x60, 0x0f, 0xe8, 0x00}},
    // Nortel VPN
    {MAC_LENGTH, {0x44, 0x45, 0x53, 0x54, 0x42, 0x00}},
    // AEP VPN
    {MAC_LENGTH, {0x00, 0x30, 0x70, 0x00, 0x00, 0x01}},
    // Positive VPN
    {MAC_LENGTH, {0x00, 0x02, 0x03, 0x04, 0x05, 0x06}},
    // Bluetooth
    {MAC_LENGTH, {0x00, 0x15, 0x83, 0x0B, 0x13, 0xC0}},
    // Kerio Virtual Network Adapter
    {MAC_LENGTH, {0x44, 0x45, 0x53, 0x54, 0x4f, 0x53}},
    // Sierra Wireless cell modems.
    {OUI_LENGTH, {0x00, 0xA0, 0xD5}},
    // FRITZ!web DSL
    {MAC_LENGTH, {0x00, 0x04, 0x0E, 0xFF, 0xFF, 0xFF}},
    // VirtualPC
    {MAC_LENGTH, {0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
    // Bluetooth
    {MAC_LENGTH, {0x00, 0x1F, 0x81, 0x00, 0x01, 0x00}},
    {MAC_LENGTH, {0x00, 0x30, 0x91, 0x10, 0x00, 0x26}},
    {MAC_LENGTH, {0x00, 0x25, 0x00, 0x5A, 0xC3, 0xD0}},
    {MAC_LENGTH, {0x00, 0x15, 0x83, 0x0C, 0xBF, 0xEB}},
    // Huawei cell modem
    {MAC_LENGTH, {0x58, 0x2C, 0x80, 0x13, 0x92, 0x63}},
    // Fortinet VPN
    {OUI_LENGTH, {0x00, 0x09, 0x0F}},
    // Realtek
    {MAC_LENGTH, {0x00, 0x00, 0x00, 0x00, 0x00, 0x30}},
    // Other rare dupes.
    {MAC_LENGTH, {0x00, 0x11, 0xf5, 0x0d, 0x8a, 0xe8}}, // Atheros
    {MAC_LENGTH, {0x00, 0x20, 0x07, 0x01, 0x16, 0x06}}, // Atheros
    {MAC_LENGTH, {0x0d, 0x0b, 0x00, 0x00, 0xe0, 0x00}}, // Atheros
    {MAC_LENGTH, {0x90, 0x4c, 0xe5, 0x0b, 0xc8, 0x8e}}, // Atheros
    {MAC_LENGTH, {0x00, 0x1c, 0x23, 0x38, 0x49, 0xa4}}, // Broadcom
    {MAC_LENGTH, {0x00, 0x12, 0x3f, 0x82, 0x7c, 0x32}}, // Broadcom
    {MAC_LENGTH, {0x00, 0x11, 0x11, 0x32, 0xc3, 0x77}}, // Broadcom
    {MAC_LENGTH, {0x00, 0x24, 0xd6, 0xae, 0x3e, 0x39}}, // Microsoft
    {MAC_LENGTH, {0x00, 0x0f, 0xb0, 0x3a, 0xb4, 0x80}}, // Realtek
    {MAC_LENGTH, {0x08, 0x10, 0x74, 0xa1, 0xda, 0x1b}}, // Realtek
    {MAC_LENGTH, {0x00, 0x21, 0x9b, 0x2a, 0x0a, 0x9c}}, // Realtek
  };

  if (size != MAC_LENGTH) {
    return false;
  }

  if (static_cast<const unsigned char *>(bytes)[0] & 0x02) {
    // Locally administered.
    return false;
  }

  // Note: Use ARRAYSIZE_UNSAFE() instead of arraysize() because InvalidMacEntry
  // is declared inside this function.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kInvalidAddresses); ++i) {
    size_t count = kInvalidAddresses[i].size;
    if (memcmp(kInvalidAddresses[i].address, bytes, count) == 0) {
        return false;
    }
  }
  return true;
}

class MacAddressProcessor {
 public:
  MacAddressProcessor() : found_index_(ULONG_MAX) {}

  // Iterate through the interfaces, looking for the valid MAC address with the
  // lowest IfIndex.
  void ProcessAdapterAddress(PIP_ADAPTER_ADDRESSES address) {
    if (address->IfType == IF_TYPE_TUNNEL)
      return;

    ProcessPhysicalAddress(address->IfIndex,
                           address->PhysicalAddress,
                           address->PhysicalAddressLength);
  }

  void ProcessInterfaceRow(const PMIB_IF_ROW2 row) {
    if (row->Type == IF_TYPE_TUNNEL ||
        !row->InterfaceAndOperStatusFlags.HardwareInterface) {
      return;
    }

    ProcessPhysicalAddress(row->InterfaceIndex,
                           row->PhysicalAddress,
                           row->PhysicalAddressLength);
  }

  std::string mac_address() const { return found_mac_address_; }

 private:
  void ProcessPhysicalAddress(NET_IFINDEX index,
                              const void* bytes,
                              size_t size) {
    if (index >= found_index_ || size == 0)
      return;

    if (!IsValidMacAddress(bytes, size))
      return;

    found_mac_address_ = StringToLowerASCII(base::HexEncode(bytes, size));
    found_index_ = index;
  }

  std::string found_mac_address_;
  NET_IFINDEX found_index_;
};

std::string GetMacAddressFromGetAdaptersAddresses() {
  base::ThreadRestrictions::AssertIOAllowed();

  // MS recommends a default size of 15k.
  ULONG bufferSize = 15 * 1024;
  // Disable as much as we can, since all we want is MAC addresses.
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER |
                GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_UNICAST;
  std::vector<unsigned char> buffer(bufferSize);
  PIP_ADAPTER_ADDRESSES adapterAddresses =
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer.front());

  DWORD result = GetAdaptersAddresses(AF_UNSPEC, flags, 0,
                                      adapterAddresses, &bufferSize);
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(bufferSize);
    adapterAddresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer.front());
    result = GetAdaptersAddresses(AF_UNSPEC, flags, 0,
                                  adapterAddresses, &bufferSize);
  }

  if (result != NO_ERROR) {
    VLOG(ERROR) << "GetAdapatersAddresses failed with error " << result;
    return "";
  }

  MacAddressProcessor processor;
  for (; adapterAddresses != NULL; adapterAddresses = adapterAddresses->Next) {
    processor.ProcessAdapterAddress(adapterAddresses);
  }
  return processor.mac_address();
}

std::string GetMacAddressFromGetIfTable2() {
  base::ThreadRestrictions::AssertIOAllowed();

  // This is available on Vista+ only.
  base::ScopedNativeLibrary library(base::FilePath(L"Iphlpapi.dll"));

  typedef DWORD (NETIOAPI_API_ *GetIfTablePtr)(PMIB_IF_TABLE2*);
  typedef void (NETIOAPI_API_ *FreeMibTablePtr)(PMIB_IF_TABLE2);

  GetIfTablePtr getIfTable = reinterpret_cast<GetIfTablePtr>(
      library.GetFunctionPointer("GetIfTable2"));
  FreeMibTablePtr freeMibTablePtr = reinterpret_cast<FreeMibTablePtr>(
      library.GetFunctionPointer("FreeMibTable"));
  if (getIfTable == NULL || freeMibTablePtr == NULL) {
    VLOG(ERROR) << "Could not get proc addresses for machine identifier.";
    return "";
  }

  PMIB_IF_TABLE2  ifTable = NULL;
  DWORD result = getIfTable(&ifTable);
  if (result != NO_ERROR || ifTable == NULL) {
    VLOG(ERROR) << "GetIfTable failed with error " << result;
    return "";
  }

  MacAddressProcessor processor;
  for (size_t i = 0; i < ifTable->NumEntries; i++) {
    processor.ProcessInterfaceRow(&(ifTable->Table[i]));
  }

  if (ifTable != NULL) {
    freeMibTablePtr(ifTable);
    ifTable = NULL;
  }
  return processor.mac_address();
}

}  // namespace

// Returns the legacy device ID which was used involuntarily to generate hashes
// in M33 (crbug.com/350028). The algorithm to compute the legacy device ID was
// duplicated from the Windows implementation of GetRawDeviceId() inside
// extensions::api::GetDeviceId() for M33 where it originated from. It was
// deemed undesirable to move the code to a common path for re-use in this code
// as GetLegacyDeviceId() is meant to forever reproduce the ID that was
// generated on M33 and shouldn't otherwise evolve with the implementation of
// extensions::api::GetDeviceId() on trunk.
// TODO(gab): Delete this method when the vast majority of the population has
// migrated over to hashes based on the modern ID.
std::string GetLegacyDeviceId(const std::string& modern_device_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Just as in extensions::api::GetMacAddressCallback(), return the
  // concatenation of |mac_address| and |modern_device_id|. Return the empty
  // string if either of them is empty.

  if (modern_device_id.empty())
    return std::string();

  // Get MAC address as in extensions::api::GetMacAddressCallback(): prefering
  // GetMacAddressFromGetAdaptersAddresses() over
  // GetMacAddressFromGetIfTable2() if the former can find a valid MAC address.
  std::string mac_address = GetMacAddressFromGetAdaptersAddresses();
  if (mac_address.empty()) {
    mac_address = GetMacAddressFromGetIfTable2();
    if (mac_address.empty())
      return std::string();
  }

  return mac_address + modern_device_id;
}
