// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

namespace device {

class HidConnection : public base::RefCountedThreadSafe<HidConnection> {
 public:
  enum SpecialReportIds {
    kNullReportId = 0x00,
    kAnyReportId = 0xFF,
  };

  typedef base::Callback<void(bool success, size_t size)> IOCallback;

  const HidDeviceInfo& device_info() const { return device_info_; }
  bool has_protected_collection() const { return has_protected_collection_; }
  bool has_report_id() const { return has_report_id_; }
  const base::ThreadChecker& thread_checker() const { return thread_checker_; }

  void Read(scoped_refptr<net::IOBufferWithSize> buffer,
            const IOCallback& callback);
  void Write(uint8_t report_id,
             scoped_refptr<net::IOBufferWithSize> buffer,
             const IOCallback& callback);
  void GetFeatureReport(uint8_t report_id,
                        scoped_refptr<net::IOBufferWithSize> buffer,
                        const IOCallback& callback);
  void SendFeatureReport(uint8_t report_id,
                         scoped_refptr<net::IOBufferWithSize> buffer,
                         const IOCallback& callback);

 protected:
  friend class base::RefCountedThreadSafe<HidConnection>;

  explicit HidConnection(const HidDeviceInfo& device_info);
  virtual ~HidConnection();

  virtual void PlatformRead(scoped_refptr<net::IOBufferWithSize> buffer,
                            const IOCallback& callback) = 0;
  virtual void PlatformWrite(uint8_t report_id,
                             scoped_refptr<net::IOBufferWithSize> buffer,
                             const IOCallback& callback) = 0;
  virtual void PlatformGetFeatureReport(
      uint8_t report_id,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const IOCallback& callback) = 0;
  virtual void PlatformSendFeatureReport(
      uint8_t report_id,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const IOCallback& callback) = 0;

  // PlatformRead implementation must call this method on read
  // success, rather than directly running the callback.
  // In case incoming buffer is empty or protected, it is filtered
  // and this method returns false. Otherwise it runs the callback
  // and returns true.
  bool CompleteRead(scoped_refptr<net::IOBufferWithSize> buffer,
                    const IOCallback& callback);

 private:
  bool IsReportIdProtected(const uint8_t report_id);

  const HidDeviceInfo device_info_;
  bool has_report_id_;
  bool has_protected_collection_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

struct PendingHidReport {
  PendingHidReport();
  ~PendingHidReport();

  scoped_refptr<net::IOBufferWithSize> buffer;
};

struct PendingHidRead {
  PendingHidRead();
  ~PendingHidRead();

  scoped_refptr<net::IOBufferWithSize> buffer;
  HidConnection::IOCallback callback;
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
