// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_WIN_H_
#define DEVICE_HID_HID_SERVICE_WIN_H_

#include <windows.h>
#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace base {
namespace win {
class MessageWindow;
}
}

namespace device {

class HidServiceWin : public HidService {
 public:
  HidServiceWin();

  virtual void Connect(const HidDeviceId& device_id,
                       const ConnectCallback& callback) override;

 private:
  virtual ~HidServiceWin();

  void RegisterForDeviceNotifications();
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);
  void DoInitialEnumeration();
  static void CollectInfoFromButtonCaps(PHIDP_PREPARSED_DATA preparsed_data,
                                        HIDP_REPORT_TYPE report_type,
                                        USHORT button_caps_length,
                                        HidCollectionInfo* collection_info);
  static void CollectInfoFromValueCaps(PHIDP_PREPARSED_DATA preparsed_data,
                                       HIDP_REPORT_TYPE report_type,
                                       USHORT value_caps_length,
                                       HidCollectionInfo* collection_info);
  void PlatformAddDevice(const std::string& device_path);
  void PlatformRemoveDevice(const std::string& device_path);

  // Tries to open the device read-write and falls back to read-only.
  base::win::ScopedHandle OpenDevice(const std::string& device_path);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<base::win::MessageWindow> window_;
  HDEVNOTIFY notify_handle_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_WIN_H_
