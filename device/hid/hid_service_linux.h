// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_LINUX_H_
#define DEVICE_HID_HID_SERVICE_LINUX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace device {

class HidServiceLinux : public HidService {
 public:
  HidServiceLinux();
  ~HidServiceLinux() override;

  // HidService:
  void Connect(const std::string& device_id,
               const ConnectCallback& callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  struct ConnectParams;
  class BlockingTaskHelper;

  // These functions implement the process of locating, requesting access to and
  // opening a device. Because this operation crosses multiple threads these
  // functions are static and the necessary parameters are passed as a single
  // struct.
#if defined(OS_CHROMEOS)
  static void OnPathOpenComplete(std::unique_ptr<ConnectParams> params,
                                 base::ScopedFD fd);
  static void OnPathOpenError(const std::string& device_path,
                              const ConnectCallback& callback,
                              const std::string& error_name,
                              const std::string& error_message);
#else
  static void OpenOnBlockingThread(std::unique_ptr<ConnectParams> params);
#endif
  static void FinishOpen(std::unique_ptr<ConnectParams> params);
  static void CreateConnection(std::unique_ptr<ConnectParams> params);

  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // |helper_| lives on the sequence |blocking_task_runner_| posts to and holds
  // a weak reference back to the service that owns it.
  std::unique_ptr<BlockingTaskHelper> helper_;
  base::WeakPtrFactory<HidServiceLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
