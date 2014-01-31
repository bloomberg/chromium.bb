// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"

namespace net {
class IOBuffer;
}

namespace device {

class HidConnection : public base::RefCountedThreadSafe<HidConnection> {
 public:
  typedef base::Callback<void(bool success, size_t size)> IOCallback;

  virtual void Read(scoped_refptr<net::IOBuffer> buffer,
                    size_t size,
                    const IOCallback& callback) = 0;
  virtual void Write(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const IOCallback& callback) = 0;
  virtual void GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                size_t size,
                                const IOCallback& callback) = 0;
  virtual void SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const IOCallback& callback) = 0;

  const HidDeviceInfo& device_info() const;

 protected:
  friend class base::RefCountedThreadSafe<HidConnection>;
  friend struct HidDeviceInfo;

  HidConnection(HidDeviceInfo device_info);
  virtual ~HidConnection();

  const HidDeviceInfo device_info_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
