// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbContext;
class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public base::MessageLoop::DestructionObserver,
                   public base::NonThreadSafe {
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

  // base::MessageLoop::DestructionObserver implementation.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 private:
  friend struct base::DefaultDeleter<UsbService>;

  explicit UsbService(PlatformUsbContext context);
  virtual ~UsbService();

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
