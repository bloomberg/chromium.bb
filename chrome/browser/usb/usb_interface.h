// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_INTERFACE_H_
#define CHROME_BROWSER_USB_USB_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

struct libusb_config_descriptor;
struct libusb_endpoint_descriptor;
struct libusb_interface;
struct libusb_interface_descriptor;

typedef libusb_config_descriptor* PlatformUsbConfigDescriptor;
typedef const libusb_endpoint_descriptor* PlatformUsbEndpointDescriptor;
typedef const libusb_interface* PlatformUsbInterface;
typedef const libusb_interface_descriptor* PlatformUsbInterfaceDescriptor;

class UsbDeviceHandle;

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

class UsbConfigDescriptor;

class UsbEndpointDescriptor : public base::RefCounted<UsbEndpointDescriptor> {
 public:
  UsbEndpointDescriptor(scoped_refptr<const UsbConfigDescriptor> config,
      PlatformUsbEndpointDescriptor descriptor);

  int GetAddress() const;
  UsbEndpointDirection GetDirection() const;
  int GetMaximumPacketSize() const;
  UsbSynchronizationType GetSynchronizationType() const;
  UsbTransferType GetTransferType() const;
  UsbUsageType GetUsageType() const;
  int GetPollingInterval() const;

 private:
  friend class base::RefCounted<UsbEndpointDescriptor>;
  ~UsbEndpointDescriptor();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbEndpointDescriptor descriptor_;
};

class UsbInterfaceDescriptor
    : public base::RefCounted<UsbInterfaceDescriptor> {
 public:
  UsbInterfaceDescriptor(scoped_refptr<const UsbConfigDescriptor> config,
      PlatformUsbInterfaceDescriptor descriptor);

  size_t GetNumEndpoints() const;
  scoped_refptr<const UsbEndpointDescriptor>
      GetEndpoint(size_t index) const;

  int GetInterfaceNumber() const;
  int GetAlternateSetting() const;
  int GetInterfaceClass() const;
  int GetInterfaceSubclass() const;
  int GetInterfaceProtocol() const;

 private:
  friend class base::RefCounted<UsbInterfaceDescriptor>;
  ~UsbInterfaceDescriptor();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbInterfaceDescriptor descriptor_;
};

class UsbInterface : public base::RefCounted<UsbInterface> {
 public:
  UsbInterface(scoped_refptr<const UsbConfigDescriptor> config,
      PlatformUsbInterface usbInterface);

  size_t GetNumAltSettings() const;
  scoped_refptr<const UsbInterfaceDescriptor>
      GetAltSetting(size_t index) const;

 private:
  friend class base::RefCounted<UsbInterface>;
  ~UsbInterface();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbInterface interface_;
};

class UsbConfigDescriptor : public base::RefCounted<UsbConfigDescriptor> {
 public:
  UsbConfigDescriptor();

  void Reset(PlatformUsbConfigDescriptor config);

  size_t GetNumInterfaces() const;

  scoped_refptr<const UsbInterface>
      GetInterface(size_t index) const;

 private:
  friend class base::RefCounted<UsbConfigDescriptor>;
  ~UsbConfigDescriptor();

  PlatformUsbConfigDescriptor config_;
};

#endif  // CHROME_BROWSER_USB_USB_INTERFACE_H_
