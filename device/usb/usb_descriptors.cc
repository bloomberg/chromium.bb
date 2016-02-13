// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "device/usb/usb_device_handle.h"
#include "net/base/io_buffer.h"

namespace device {

namespace {

using IndexMap = std::map<uint8_t, base::string16>;
using IndexMapPtr = scoped_ptr<IndexMap>;

// Standard USB requests and descriptor types:
const uint8_t kGetDescriptorRequest = 0x06;
const uint8_t kStringDescriptorType = 0x03;

const int kControlTransferTimeout = 60000;  // 1 minute

struct UsbInterfaceAssociationDescriptor {
  UsbInterfaceAssociationDescriptor(uint8_t first_interface,
                                    uint8_t interface_count)
      : first_interface(first_interface), interface_count(interface_count) {}

  bool operator<(const UsbInterfaceAssociationDescriptor& other) const {
    return first_interface < other.first_interface;
  }

  uint8_t first_interface;
  uint8_t interface_count;
};

void ParseInterfaceAssociationDescriptors(
    const std::vector<uint8_t>& buffer,
    std::vector<UsbInterfaceAssociationDescriptor>* functions) {
  const uint8_t kInterfaceAssociationDescriptorType = 11;
  const uint8_t kInterfaceAssociationDescriptorLength = 8;
  std::vector<uint8_t>::const_iterator it = buffer.begin();

  while (it != buffer.end()) {
    // All descriptors must be at least 2 byte which means the length and type
    // are safe to read.
    if (std::distance(it, buffer.end()) < 2)
      return;
    uint8_t length = it[0];
    if (length > std::distance(it, buffer.end()))
      return;
    if (it[1] == kInterfaceAssociationDescriptorType &&
        length == kInterfaceAssociationDescriptorLength) {
      functions->push_back(UsbInterfaceAssociationDescriptor(it[2], it[3]));
    }
    std::advance(it, length);
  }
}

void StoreStringDescriptor(IndexMap::iterator it,
                           const base::Closure& callback,
                           const base::string16& string) {
  it->second = string;
  callback.Run();
}

void OnReadStringDescriptor(
    const base::Callback<void(const base::string16&)>& callback,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length) {
  base::string16 string;
  if (status == USB_TRANSFER_COMPLETED &&
      ParseUsbStringDescriptor(
          std::vector<uint8_t>(buffer->data(), buffer->data() + length),
          &string)) {
    callback.Run(string);
  } else {
    callback.Run(base::string16());
  }
}

void ReadStringDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8_t index,
    uint16_t language_id,
    const base::Callback<void(const base::string16&)>& callback) {
  scoped_refptr<net::IOBufferWithSize> buffer = new net::IOBufferWithSize(255);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD, UsbDeviceHandle::DEVICE,
      kGetDescriptorRequest, kStringDescriptorType << 8 | index, language_id,
      buffer, buffer->size(), kControlTransferTimeout,
      base::Bind(&OnReadStringDescriptor, callback));
}

void OnReadLanguageIds(scoped_refptr<UsbDeviceHandle> device_handle,
                       IndexMapPtr index_map,
                       const base::Callback<void(IndexMapPtr)>& callback,
                       const base::string16& languages) {
  // Default to English unless the device provides a language and then just pick
  // the first one.
  uint16_t language_id = languages.empty() ? 0x0409 : languages[0];

  std::map<uint8_t, IndexMap::iterator> iterator_map;
  for (auto it = index_map->begin(); it != index_map->end(); ++it)
    iterator_map[it->first] = it;

  base::Closure barrier =
      base::BarrierClosure(static_cast<int>(iterator_map.size()),
                           base::Bind(callback, base::Passed(&index_map)));
  for (const auto& map_entry : iterator_map) {
    ReadStringDescriptor(
        device_handle, map_entry.first, language_id,
        base::Bind(&StoreStringDescriptor, map_entry.second, barrier));
  }
}

}  // namespace

UsbEndpointDescriptor::UsbEndpointDescriptor(
    uint8_t address,
    UsbEndpointDirection direction,
    uint16_t maximum_packet_size,
    UsbSynchronizationType synchronization_type,
    UsbTransferType transfer_type,
    UsbUsageType usage_type,
    uint16_t polling_interval)
    : address(address),
      direction(direction),
      maximum_packet_size(maximum_packet_size),
      synchronization_type(synchronization_type),
      transfer_type(transfer_type),
      usage_type(usage_type),
      polling_interval(polling_interval) {}

UsbEndpointDescriptor::~UsbEndpointDescriptor() = default;

UsbInterfaceDescriptor::UsbInterfaceDescriptor(uint8_t interface_number,
                                               uint8_t alternate_setting,
                                               uint8_t interface_class,
                                               uint8_t interface_subclass,
                                               uint8_t interface_protocol)
    : interface_number(interface_number),
      alternate_setting(alternate_setting),
      interface_class(interface_class),
      interface_subclass(interface_subclass),
      interface_protocol(interface_protocol),
      first_interface(interface_number) {}

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() = default;

UsbConfigDescriptor::UsbConfigDescriptor(uint8_t configuration_value,
                                         bool self_powered,
                                         bool remote_wakeup,
                                         uint16_t maximum_power)
    : configuration_value(configuration_value),
      self_powered(self_powered),
      remote_wakeup(remote_wakeup),
      maximum_power(maximum_power) {}

UsbConfigDescriptor::~UsbConfigDescriptor() = default;

void UsbConfigDescriptor::AssignFirstInterfaceNumbers() {
  std::vector<UsbInterfaceAssociationDescriptor> functions;
  ParseInterfaceAssociationDescriptors(extra_data, &functions);
  for (const auto& interface : interfaces) {
    ParseInterfaceAssociationDescriptors(interface.extra_data, &functions);
    for (const auto& endpoint : interface.endpoints)
      ParseInterfaceAssociationDescriptors(endpoint.extra_data, &functions);
  }

  // libusb has collected interface association descriptors in the |extra_data|
  // fields of other descriptor types. This may have disturbed their order
  // but sorting by the bFirstInterface should fix it.
  std::sort(functions.begin(), functions.end());

  uint8_t remaining_interfaces = 0;
  auto function_it = functions.cbegin();
  for (auto interface_it = interfaces.begin(); interface_it != interfaces.end();
       ++interface_it) {
    if (remaining_interfaces > 0) {
      // Continuation of a previous function. Tag all alternate interfaces
      // (which are guaranteed to be contiguous).
      for (uint8_t interface_number = interface_it->interface_number;
           interface_it != interfaces.end() &&
           interface_it->interface_number == interface_number;
           ++interface_it) {
        interface_it->first_interface = function_it->first_interface;
      }
      if (--remaining_interfaces == 0)
        ++function_it;
    } else if (function_it != functions.end() &&
               interface_it->interface_number == function_it->first_interface) {
      // Start of a new function.
      interface_it->first_interface = function_it->first_interface;
      remaining_interfaces = function_it->interface_count - 1;
    } else {
      // Unassociated interfaces already have |first_interface| set to
      // |interface_number|.
    }
  }
}

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output) {
  if (descriptor.size() < 2 || descriptor[1] != kStringDescriptorType)
    return false;

  // Let the device return a buffer larger than the actual string but prefer the
  // length reported inside the descriptor.
  size_t length = descriptor[0];
  length = std::min(length, descriptor.size());
  if (length < 2)
    return false;

  // The string is returned by the device in UTF-16LE.
  *output = base::string16(
      reinterpret_cast<const base::char16*>(descriptor.data() + 2),
      length / 2 - 1);
  return true;
}

// For each key in |index_map| this function reads that string descriptor from
// |device_handle| and updates the value in in |index_map|.
void ReadUsbStringDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    IndexMapPtr index_map,
    const base::Callback<void(IndexMapPtr)>& callback) {
  if (index_map->empty()) {
    callback.Run(std::move(index_map));
    return;
  }

  ReadStringDescriptor(device_handle, 0, 0,
                       base::Bind(&OnReadLanguageIds, device_handle,
                                  base::Passed(&index_map), callback));
}

}  // namespace device
