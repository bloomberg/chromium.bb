// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_INTERFACE_H_
#define DEVICE_USB_USB_INTERFACE_H_

#include "base/memory/ref_counted.h"

namespace device {

enum UsbTransferType {
  USB_TRANSFER_CONTROL = 0,
  USB_TRANSFER_ISOCHRONOUS,
  USB_TRANSFER_BULK,
  USB_TRANSFER_INTERRUPT,
};

enum UsbEndpointDirection {
  USB_DIRECTION_INBOUND = 0,
  USB_DIRECTION_OUTBOUND,
};

enum UsbSynchronizationType {
  USB_SYNCHRONIZATION_NONE = 0,
  USB_SYNCHRONIZATION_ASYNCHRONOUS,
  USB_SYNCHRONIZATION_ADAPTIVE,
  USB_SYNCHRONIZATION_SYNCHRONOUS,
};

enum UsbUsageType {
  USB_USAGE_DATA = 0,
  USB_USAGE_FEEDBACK,
  USB_USAGE_EXPLICIT_FEEDBACK
};

class UsbEndpointDescriptor
    : public base::RefCounted<const UsbEndpointDescriptor> {
 public:
  virtual int GetAddress() const = 0;
  virtual UsbEndpointDirection GetDirection() const = 0;
  virtual int GetMaximumPacketSize() const = 0;
  virtual UsbSynchronizationType GetSynchronizationType() const = 0;
  virtual UsbTransferType GetTransferType() const = 0;
  virtual UsbUsageType GetUsageType() const = 0;
  virtual int GetPollingInterval() const = 0;

 protected:
  friend class base::RefCounted<const UsbEndpointDescriptor>;

  UsbEndpointDescriptor() {};
  virtual ~UsbEndpointDescriptor() {};

  DISALLOW_COPY_AND_ASSIGN(UsbEndpointDescriptor);
};

class UsbInterfaceAltSettingDescriptor
    : public base::RefCounted<const UsbInterfaceAltSettingDescriptor> {
 public:
  virtual size_t GetNumEndpoints() const = 0;
  virtual scoped_refptr<const UsbEndpointDescriptor> GetEndpoint(
      size_t index) const = 0;

  virtual int GetInterfaceNumber() const = 0;
  virtual int GetAlternateSetting() const = 0;
  virtual int GetInterfaceClass() const = 0;
  virtual int GetInterfaceSubclass() const = 0;
  virtual int GetInterfaceProtocol() const = 0;

 protected:
  friend class base::RefCounted<const UsbInterfaceAltSettingDescriptor>;

  UsbInterfaceAltSettingDescriptor() {};
  virtual ~UsbInterfaceAltSettingDescriptor() {};

  DISALLOW_COPY_AND_ASSIGN(UsbInterfaceAltSettingDescriptor);
};

class UsbInterfaceDescriptor
    : public base::RefCounted<const UsbInterfaceDescriptor> {
 public:
  virtual size_t GetNumAltSettings() const = 0;
  virtual scoped_refptr<const UsbInterfaceAltSettingDescriptor> GetAltSetting(
      size_t index) const = 0;

 protected:
  friend class base::RefCounted<const UsbInterfaceDescriptor>;

  UsbInterfaceDescriptor() {};
  virtual ~UsbInterfaceDescriptor() {};

  DISALLOW_COPY_AND_ASSIGN(UsbInterfaceDescriptor);
};

class UsbConfigDescriptor : public base::RefCounted<UsbConfigDescriptor> {
 public:
  virtual size_t GetNumInterfaces() const = 0;
  virtual scoped_refptr<const UsbInterfaceDescriptor> GetInterface(
      size_t index) const = 0;

 protected:
  friend class base::RefCounted<UsbConfigDescriptor>;

  UsbConfigDescriptor() {};
  virtual ~UsbConfigDescriptor() {};

  DISALLOW_COPY_AND_ASSIGN(UsbConfigDescriptor);
};

}  // namespace device

#endif  // DEVICE_USB_USB_INTERFACE_H_
