// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_LINUX_H_
#define DEVICE_HID_HID_SERVICE_LINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace device {

class HidConnection;

class HidServiceLinux : public HidService {
 public:
  HidServiceLinux(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  void Connect(const HidDeviceId& device_id,
               const ConnectCallback& callback) override;

 private:
  struct ConnectParams;
  class FileThreadHelper;

  ~HidServiceLinux() override;

  // Constructs this services helper object that lives on the FILE thread.
  static void StartHelper(
      base::WeakPtr<HidServiceLinux> weak_ptr,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // These functions implement the process of locating, requesting access to and
  // opening a device. Because this operation crosses multiple threads these
  // functions are static and the necessary parameters are passed as a single
  // struct.
#if defined(OS_CHROMEOS)
  static void OnRequestPathAccessComplete(scoped_ptr<ConnectParams> params,
                                          bool success);
#endif  // defined(OS_CHROMEOS)
  static void OpenDevice(scoped_ptr<ConnectParams> params);
  static void ConnectImpl(scoped_ptr<ConnectParams> params);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // The helper lives on the FILE thread and holds a weak reference back to the
  // service that owns it.
  scoped_ptr<FileThreadHelper> helper_;
  base::WeakPtrFactory<HidServiceLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
