// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "third_party/libusb/src/libusb/libusb.h"

class UsbEventHandler;
template <typename T> struct DefaultSingletonTraits;
struct libusb_context;

typedef struct libusb_context* PlatformUsbContext;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsbile for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService {
 public:
  static UsbService* GetInstance();

  // Find all of the devices attached to the system that are identified by
  // |vendor_id| and |product_id|, inserting them into |devices|. Clears
  // |devices| before use. Calls |callback| once |devices| is populated.
  void FindDevices(const uint16 vendor_id,
                   const uint16 product_id,
                   int interface_id,
                   std::vector<scoped_refptr<UsbDeviceHandle> >* devices,
                   const base::Callback<void()>& callback);

  // Find all of the devices attached to the system, inserting them into
  // |devices|. Clears |devices| before use.
  void EnumerateDevices(std::vector<scoped_refptr<UsbDeviceHandle> >* devices);

  // This function should not be called by normal code. It is invoked by a
  // UsbDevice's Close function and disposes of the associated platform handle.
  void CloseDevice(scoped_refptr<UsbDeviceHandle> device);

 protected:
  UsbService();
  virtual ~UsbService();

 private:
  friend struct DefaultSingletonTraits<UsbService>;


  // RefCountedPlatformUsbDevice takes care of managing the underlying reference
  // count of a single PlatformUsbDevice. This allows us to construct things
  // like vectors of RefCountedPlatformUsbDevices and not worry about having to
  // explicitly unreference them after use.
  class RefCountedPlatformUsbDevice {
   public:
    explicit RefCountedPlatformUsbDevice(PlatformUsbDevice device);
    RefCountedPlatformUsbDevice(const RefCountedPlatformUsbDevice& other);
    virtual ~RefCountedPlatformUsbDevice();
    PlatformUsbDevice device();

   private:
    PlatformUsbDevice device_;
  };

  typedef std::vector<RefCountedPlatformUsbDevice> DeviceVector;

  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(PlatformUsbDevice device,
                            const uint16 vendor_id,
                            const uint16 product_id);

  // FindDevicesImpl is called by FindDevices on ChromeOS after the permission
  // broker has signalled that permission has been granted to access the
  // underlying device nodes. On other platforms, it is called directly by
  // FindDevices.
  void FindDevicesImpl(const uint16 vendor_id,
                       const uint16 product_id,
                       std::vector<scoped_refptr<UsbDeviceHandle> >* devices,
                       const base::Callback<void()>& callback,
                       bool success);

  // Populates |output| with the result of enumerating all attached USB devices.
  void EnumerateDevicesImpl(DeviceVector* output);

  // If a UsbDevice wrapper corresponding to |device| has already been created,
  // returns it. Otherwise, opens the device, creates a wrapper, and associates
  // the wrapper with the device internally.
  UsbDeviceHandle* LookupOrCreateDevice(PlatformUsbDevice device);

  PlatformUsbContext context_;
  UsbEventHandler* event_handler_;

  // The devices_ map contains scoped_refptrs to all open devices, indicated by
  // their vendor and product id. This allows for reusing an open device without
  // creating another platform handle for it.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceHandle> >
      DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
