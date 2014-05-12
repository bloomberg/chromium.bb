// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_INTERFACE_IMPL_H_
#define COMPONENTS_USB_SERVICE_USB_INTERFACE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/usb_service/usb_interface.h"
#include "components/usb_service/usb_service_export.h"

struct libusb_config_descriptor;
struct libusb_endpoint_descriptor;
struct libusb_interface;
struct libusb_interface_descriptor;

namespace usb_service {

typedef libusb_config_descriptor* PlatformUsbConfigDescriptor;
typedef const libusb_endpoint_descriptor* PlatformUsbEndpointDescriptor;
typedef const libusb_interface* PlatformUsbInterface;
typedef const libusb_interface_descriptor* PlatformUsbInterfaceDescriptor;

class UsbConfigDescriptorImpl;
class UsbInterfaceAltSettingDescriptor;

class UsbEndpointDescriptorImpl : public UsbEndpointDescriptor {
 public:
  virtual int GetAddress() const OVERRIDE;
  virtual UsbEndpointDirection GetDirection() const OVERRIDE;
  virtual int GetMaximumPacketSize() const OVERRIDE;
  virtual UsbSynchronizationType GetSynchronizationType() const OVERRIDE;
  virtual UsbTransferType GetTransferType() const OVERRIDE;
  virtual UsbUsageType GetUsageType() const OVERRIDE;
  virtual int GetPollingInterval() const OVERRIDE;

 private:
  friend class base::RefCounted<const UsbEndpointDescriptorImpl>;
  friend class UsbInterfaceAltSettingDescriptorImpl;

  UsbEndpointDescriptorImpl(scoped_refptr<const UsbConfigDescriptor> config,
                        PlatformUsbEndpointDescriptor descriptor);
  virtual ~UsbEndpointDescriptorImpl();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbEndpointDescriptor descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbEndpointDescriptorImpl);
};

class UsbInterfaceAltSettingDescriptorImpl
    : public UsbInterfaceAltSettingDescriptor {
 public:
  virtual size_t GetNumEndpoints() const OVERRIDE;
  virtual scoped_refptr<const UsbEndpointDescriptor> GetEndpoint(
      size_t index) const OVERRIDE;

  virtual int GetInterfaceNumber() const OVERRIDE;
  virtual int GetAlternateSetting() const OVERRIDE;
  virtual int GetInterfaceClass() const OVERRIDE;
  virtual int GetInterfaceSubclass() const OVERRIDE;
  virtual int GetInterfaceProtocol() const OVERRIDE;

 private:
  friend class UsbInterfaceDescriptorImpl;

  UsbInterfaceAltSettingDescriptorImpl(
      scoped_refptr<const UsbConfigDescriptor> config,
      PlatformUsbInterfaceDescriptor descriptor);
  virtual ~UsbInterfaceAltSettingDescriptorImpl();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbInterfaceDescriptor descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbInterfaceAltSettingDescriptorImpl);
};

class UsbInterfaceDescriptorImpl : public UsbInterfaceDescriptor {
 public:
  virtual size_t GetNumAltSettings() const OVERRIDE;
  virtual scoped_refptr<const UsbInterfaceAltSettingDescriptor> GetAltSetting(
      size_t index) const OVERRIDE;

 private:
  friend class base::RefCounted<const UsbInterfaceDescriptorImpl>;
  friend class UsbConfigDescriptorImpl;

  UsbInterfaceDescriptorImpl(scoped_refptr<const UsbConfigDescriptor> config,
                             PlatformUsbInterface usbInterface);
  virtual ~UsbInterfaceDescriptorImpl();

  scoped_refptr<const UsbConfigDescriptor> config_;
  PlatformUsbInterface interface_;

  DISALLOW_COPY_AND_ASSIGN(UsbInterfaceDescriptorImpl);
};

class UsbConfigDescriptorImpl : public UsbConfigDescriptor {
 public:
  virtual size_t GetNumInterfaces() const OVERRIDE;
  virtual scoped_refptr<const UsbInterfaceDescriptor> GetInterface(
      size_t index) const OVERRIDE;

 private:
  friend class base::RefCounted<UsbConfigDescriptor>;
  friend class UsbDeviceImpl;

  explicit UsbConfigDescriptorImpl(PlatformUsbConfigDescriptor config);
  virtual ~UsbConfigDescriptorImpl();

  PlatformUsbConfigDescriptor config_;

  DISALLOW_COPY_AND_ASSIGN(UsbConfigDescriptorImpl);
};

}  // namespace usb_service;

#endif  // COMPONENTS_USB_SERVICE_USB_INTERFACE_IMPL_H_
