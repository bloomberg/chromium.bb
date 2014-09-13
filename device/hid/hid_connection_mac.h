// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_MAC_H_
#define DEVICE_HID_HID_CONNECTION_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#include <queue>

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_ptr.h"
#include "device/hid/hid_connection.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class IOBuffer;
}

namespace device {

class HidConnectionMac : public HidConnection {
 public:
  explicit HidConnectionMac(HidDeviceInfo device_info);

 private:
  virtual ~HidConnectionMac();

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

  static void InputReportCallback(void* context,
                                  IOReturn result,
                                  void* sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t* report_bytes,
                                  CFIndex report_length);

  void WriteReport(IOHIDReportType type,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t size,
                   const WriteCallback& callback);

  void Flush();
  void ProcessInputReport(scoped_refptr<net::IOBufferWithSize> buffer);
  void ProcessReadQueue();

  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  std::vector<uint8_t> inbound_buffer_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_MAC_H_
