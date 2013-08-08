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
#include "chrome/browser/usb/usb_device.h"

namespace base {

template <class T> class DeleteHelper;

}  // namespace base

template <typename T> struct DefaultSingletonTraits;
class UsbContext;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsbile for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService {
 public:
  typedef scoped_ptr<std::vector<scoped_refptr<UsbDevice> > >
      ScopedDeviceVector;

  // Must be called on FILE thread.
  static UsbService* GetInstance();

  // Find all of the devices attached to the system that are identified by
  // |vendor_id| and |product_id|, inserting them into |devices|. Clears
  // |devices| before use. Calls |callback| once |devices| is populated.
  // The result will be sorted by id in increasing order. Must be called on
  // FILE thread.
  void FindDevices(
      const uint16 vendor_id,
      const uint16 product_id,
      int interface_id,
      const base::Callback<void(ScopedDeviceVector vector)>& callback);

  // Get all of the devices attached to the system, inserting them into
  // |devices|. Clears |devices| before use. The result will be sorted by id
  // in increasing order. Must be called on FILE thread.
  void GetDevices(std::vector<scoped_refptr<UsbDevice> >* devices);

 private:
  friend struct DefaultSingletonTraits<UsbService>;
  friend class base::DeleteHelper<UsbService>;

  UsbService();
  virtual ~UsbService();

  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(scoped_refptr<UsbDevice> device,
                            const uint16 vendor_id,
                            const uint16 product_id);

  // This method is called when permission broker replied our request.
  // We will simply relay it to FILE thread.
  void OnRequestUsbAccessReplied(
      const uint16 vendor_id,
      const uint16 product_id,
      const base::Callback<void(ScopedDeviceVector vector)>& callback,
      bool success);

  // FindDevicesImpl is called by FindDevices on ChromeOS after the permission
  // broker has signaled that permission has been granted to access the
  // underlying device nodes. On other platforms, it is called directly by
  // FindDevices.
  void FindDevicesImpl(
      const uint16 vendor_id,
      const uint16 product_id,
      const base::Callback<void(ScopedDeviceVector vector)>& callback,
      bool success);

  // Enumerate USB devices from OS and Update devices_ map.
  void RefreshDevices();

  scoped_refptr<UsbContext> context_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDevice> > DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
