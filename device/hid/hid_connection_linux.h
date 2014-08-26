// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_LINUX_H_
#define DEVICE_HID_HID_CONNECTION_LINUX_H_

#include <queue>

#include "base/files/file.h"
#include "base/message_loop/message_pump_libevent.h"
#include "device/hid/hid_connection.h"

namespace device {

class HidConnectionLinux : public HidConnection,
                           public base::MessagePumpLibevent::Watcher {
 public:
  HidConnectionLinux(HidDeviceInfo device_info, std::string dev_node);

 private:
  friend class base::RefCountedThreadSafe<HidConnectionLinux>;
  virtual ~HidConnectionLinux();

  // HidConnection implementation.
  virtual void PlatformRead(const ReadCallback& callback) OVERRIDE;
  virtual void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                             size_t size,
                             const WriteCallback& callback) OVERRIDE;
  virtual void PlatformGetFeatureReport(uint8_t report_id,
                                        const ReadCallback& callback) OVERRIDE;
  virtual void PlatformSendFeatureReport(
      scoped_refptr<net::IOBuffer> buffer,
      size_t size,
      const WriteCallback& callback) OVERRIDE;

  // base::MessagePumpLibevent::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  void Disconnect();

  void Flush();
  void ProcessInputReport(scoped_refptr<net::IOBuffer> buffer, size_t size);
  void ProcessReadQueue();

  base::File device_file_;
  base::MessagePumpLibevent::FileDescriptorWatcher device_file_watcher_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_LINUX_H_
