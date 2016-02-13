// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_H_
#define DEVICE_USB_USB_DEVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "device/usb/usb_descriptors.h"
#include "url/gurl.h"

namespace device {

class UsbDeviceHandle;
struct WebUsbAllowedOrigins;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. Methods other than simple property accessors must be
// called from the thread on which this object was created. For further
// manipulation of the device, a UsbDeviceHandle must be created from Open()
// method.
class UsbDevice : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  using OpenCallback = base::Callback<void(scoped_refptr<UsbDeviceHandle>)>;
  using ResultCallback = base::Callback<void(bool success)>;

  // This observer interface should be used by objects that need only be
  // notified about the removal of a particular device as it is more efficient
  // than registering a large number of observers with UsbService::AddObserver.
  class Observer {
   public:
    virtual ~Observer();

    // This method is called when the UsbService that created this object
    // detects that the device has been disconnected from the host.
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
  };

  // A unique identifier which remains stable for the lifetime of this device
  // object (i.e., until the device is unplugged or the USB service dies.)
  const std::string& guid() const { return guid_; }

  // Accessors to basic information.
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  const base::string16& manufacturer_string() const {
    return manufacturer_string_;
  }
  const base::string16& product_string() const { return product_string_; }
  const base::string16& serial_number() const { return serial_number_; }
  const WebUsbAllowedOrigins* webusb_allowed_origins() const {
    return webusb_allowed_origins_.get();
  }
  const GURL& webusb_landing_page() const { return webusb_landing_page_; }
  const std::vector<UsbConfigDescriptor>& configurations() const {
    return configurations_;
  }

  // On ChromeOS the permission_broker service is used to change the ownership
  // of USB device nodes so that Chrome can open them. On other platforms these
  // functions are no-ops and always return true.
  virtual void CheckUsbAccess(const ResultCallback& callback);

  // Creates a UsbDeviceHandle for further manipulation.
  virtual void Open(const OpenCallback& callback) = 0;

  // Gets the UsbConfigDescriptor for the active device configuration or nullptr
  // if the device is unconfigured.
  virtual const UsbConfigDescriptor* GetActiveConfiguration() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class UsbService;

  UsbDevice(uint16_t vendor_id,
            uint16_t product_id,
            const base::string16& manufacturer_string,
            const base::string16& product_string,
            const base::string16& serial_number);
  virtual ~UsbDevice();

  void NotifyDeviceRemoved();

  // These members must be mutable by subclasses as necessary during device
  // enumeration. To preserve the thread safety of this object they must remain
  // constant afterwards.
  base::string16 manufacturer_string_;
  base::string16 product_string_;
  base::string16 serial_number_;
  scoped_ptr<WebUsbAllowedOrigins> webusb_allowed_origins_;
  GURL webusb_landing_page_;

  // All of the device's configuration descriptors.
  std::vector<UsbConfigDescriptor> configurations_;

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;

  const std::string guid_;
  const uint16_t vendor_id_;
  const uint16_t product_id_;

  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_H_
