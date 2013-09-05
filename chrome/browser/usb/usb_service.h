// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"

namespace base {

template <class T> class DeleteHelper;

}  // namespace base

struct InitUsbContextTraits;
template <typename T> struct DefaultSingletonTraits;

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbContext;
class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService {
 public:
  typedef scoped_ptr<std::vector<scoped_refptr<UsbDevice> > >
      ScopedDeviceVector;

  // Must be called on FILE thread.
  // Returns NULL when failed to initialized.
  static UsbService* GetInstance();

  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id);

  // Get all of the devices attached to the system, inserting them into
  // |devices|. Clears |devices| before use. The result will be sorted by id
  // in increasing order. Must be called on FILE thread.
  void GetDevices(std::vector<scoped_refptr<UsbDevice> >* devices);

 private:
  friend struct InitUsbContextTraits;
  friend struct DefaultSingletonTraits<UsbService>;
  friend class base::DeleteHelper<UsbService>;

  explicit UsbService(PlatformUsbContext context);
  virtual ~UsbService();

  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(scoped_refptr<UsbDevice> device,
                            const uint16 vendor_id,
                            const uint16 product_id);

  // Enumerate USB devices from OS and Update devices_ map.
  void RefreshDevices();

  scoped_refptr<UsbContext> context_;

  // TODO(ikarienator): Figure out a better solution.
  uint32 next_unique_id_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDevice> > DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
