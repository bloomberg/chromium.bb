// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_MAC_H_
#define DEVICE_HID_HID_CONNECTION_MAC_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/tuple.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service_mac.h"

namespace net {
class IOBuffer;
}

namespace device {

class HidConnectionMac : public HidConnection {
 public:
  HidConnectionMac(HidServiceMac* service,
                   HidDeviceInfo device_info,
                   IOHIDDeviceRef device);

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

 private:
  virtual ~HidConnectionMac();

  static void InputReportCallback(void * context,
                                  IOReturn result,
                                  void * sender,
                                  IOHIDReportType type,
                                  uint32_t reportID,
                                  uint8_t * report,
                                  CFIndex reportLength);
  void ProcessReadQueue();
  void ProcessInputReport(IOHIDReportType type,
                          scoped_refptr<net::IOBuffer> report,
                          CFIndex reportLength);

  void WriteReport(IOHIDReportType type,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t size,
                   const IOCallback& callback);

  HidServiceMac* service_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  scoped_ptr_malloc<uint8_t> inbound_buffer_;
  bool disconnected_;

  typedef std::pair<scoped_refptr<net::IOBuffer>, size_t> PendingReport;
  std::queue<PendingReport> input_reports_;
  typedef Tuple3<scoped_refptr<net::IOBuffer>, size_t, IOCallback> PendingRead;
  std::queue<PendingRead> read_queue_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};


}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_MAC_H_
