// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_MAC_H_
#define DEVICE_HID_HID_SERVICE_MAC_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

namespace device {

class HidConnection;
class HidService;

class HidServiceMac : public HidService {
 public:
  HidServiceMac();

  virtual scoped_refptr<HidConnection> Connect(std::string device_id) OVERRIDE;

 private:
  virtual ~HidServiceMac();

  void ScheduleRunLoop();
  void UnscheduleRunLoop();

  // Device changing callbacks.
  static void AddDeviceCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef ref);
  static void RemoveDeviceCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef ref);
  static HidServiceMac* InstanceFromContext(void* context);

  IOHIDDeviceRef FindDevice(std::string id);

  void Enumerate();

  void PlatformAddDevice(IOHIDDeviceRef ref);
  void PlatformRemoveDevice(IOHIDDeviceRef ref);

  // The message loop this object belongs to.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // Platform HID Manager
  base::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_ref_;

  // Enumeration thread.
  scoped_ptr<base::Thread> enumeration_runloop_thread_;
  CFRunLoopRef enumeration_runloop_;
  base::WaitableEvent enumeration_runloop_init_;

  bool available_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_MAC_H_
