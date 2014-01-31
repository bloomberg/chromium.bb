// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_LINUX_H_
#define DEVICE_HID_HID_CONNECTION_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/tuple.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service_linux.h"
#include "net/base/io_buffer.h"

namespace device {

class HidConnectionLinux : public HidConnection,
                           public base::MessagePumpLibevent::Watcher {
 public:
  HidConnectionLinux(HidDeviceInfo device_info,
                     ScopedUdevDevicePtr udev_raw_device);

  virtual void Read(scoped_refptr<net::IOBuffer> buffer,
                    size_t size,
                    const IOCallback& callback) OVERRIDE;
  virtual void Write(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const IOCallback& callback) OVERRIDE;
  virtual void GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                size_t size,
                                const IOCallback& callback) OVERRIDE;
  virtual void SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const IOCallback& callback) OVERRIDE;

  bool initialized() const { return initialized_; }

  // Implements base::MessagePumpLibevent::Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<HidConnectionLinux>;
  virtual ~HidConnectionLinux();

  static bool FindHidrawDevNode(udev_device* parent, std::string* result);

  void ProcessReadQueue();
  void Disconnect();

  base::PlatformFile device_file_;
  base::MessagePumpLibevent::FileDescriptorWatcher device_file_watcher_;

  typedef std::pair<scoped_refptr<net::IOBuffer>, size_t> PendingReport;
  typedef Tuple3<scoped_refptr<net::IOBuffer>, size_t, IOCallback>
      PendingRequest;

  std::queue<PendingReport> input_reports_;
  std::queue<PendingRequest> read_queue_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_LINUX__
