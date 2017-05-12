// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_LINUX_H_
#define DEVICE_HID_HID_CONNECTION_LINUX_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/hid/hid_connection.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class HidConnectionLinux : public HidConnection {
 public:
  HidConnectionLinux(
      scoped_refptr<HidDeviceInfo> device_info,
      base::ScopedFD fd,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

 private:
  friend class base::RefCountedThreadSafe<HidConnectionLinux>;
  class BlockingTaskHelper;

  ~HidConnectionLinux() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformRead(const ReadCallback& callback) override;
  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const WriteCallback& callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                const ReadCallback& callback) override;
  void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const WriteCallback& callback) override;

  void ProcessInputReport(scoped_refptr<net::IOBuffer> buffer, size_t size);
  void ProcessReadQueue();

  // |helper_| lives on the sequence to which |blocking_task_runner_| posts
  // tasks so all calls must be posted there including this object's
  // destruction.
  std::unique_ptr<BlockingTaskHelper> helper_;

  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HidConnectionLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_LINUX_H_
