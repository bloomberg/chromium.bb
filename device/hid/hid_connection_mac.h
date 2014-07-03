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

  // HidConnection implementation.
  virtual void PlatformRead(scoped_refptr<net::IOBufferWithSize> buffer,
                            const IOCallback& callback) OVERRIDE;
  virtual void PlatformWrite(uint8_t report_id,
                             scoped_refptr<net::IOBufferWithSize> buffer,
                             const IOCallback& callback) OVERRIDE;
  virtual void PlatformGetFeatureReport(
      uint8_t report_id,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const IOCallback& callback) OVERRIDE;
  virtual void PlatformSendFeatureReport(
      uint8_t report_id,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const IOCallback& callback) OVERRIDE;

 private:
  virtual ~HidConnectionMac();

  static void InputReportCallback(void* context,
                                  IOReturn result,
                                  void* sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t* report_bytes,
                                  CFIndex report_length);

  void WriteReport(IOHIDReportType type,
                   uint8_t report_id,
                   scoped_refptr<net::IOBufferWithSize> buffer,
                   const IOCallback& callback);

  void Flush();
  void ProcessInputReport(scoped_refptr<net::IOBufferWithSize> buffer);
  void ProcessReadQueue();

  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_ptr<uint8_t, base::FreeDeleter> inbound_buffer_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_MAC_H_
