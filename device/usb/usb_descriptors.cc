// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

#include <stddef.h>

#include <algorithm>

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

UsbEndpointDescriptor::UsbEndpointDescriptor()
    : address(0),
      direction(USB_DIRECTION_INBOUND),
      maximum_packet_size(0),
      synchronization_type(USB_SYNCHRONIZATION_NONE),
      transfer_type(USB_TRANSFER_CONTROL),
      usage_type(USB_USAGE_DATA),
      polling_interval(0) {
}

UsbEndpointDescriptor::~UsbEndpointDescriptor() {
}

UsbInterfaceDescriptor::UsbInterfaceDescriptor()
    : interface_number(0),
      alternate_setting(0),
      interface_class(0),
      interface_subclass(0),
      interface_protocol(0) {
}

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() {
}

UsbConfigDescriptor::UsbConfigDescriptor()
    : configuration_value(0),
      self_powered(false),
      remote_wakeup(false),
      maximum_power(0) {
}

UsbConfigDescriptor::~UsbConfigDescriptor() {
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
