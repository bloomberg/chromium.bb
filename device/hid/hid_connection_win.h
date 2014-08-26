// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_WIN_H_
#define DEVICE_HID_HID_CONNECTION_WIN_H_

#include <windows.h>

#include <set>

#include "base/win/scoped_handle.h"
#include "device/hid/hid_connection.h"

namespace device {

struct PendingHidTransfer;

class HidConnectionWin : public HidConnection {
 public:
  explicit HidConnectionWin(const HidDeviceInfo& device_info);

 private:
  friend class HidServiceWin;
  friend struct PendingHidTransfer;

  ~HidConnectionWin();

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

  bool available() const { return file_.IsValid(); }

  void OnReadComplete(scoped_refptr<net::IOBuffer> buffer,
                      const ReadCallback& callback,
                      PendingHidTransfer* transfer,
                      bool signaled);
  void OnReadFeatureComplete(scoped_refptr<net::IOBuffer> buffer,
                             const ReadCallback& callback,
                             PendingHidTransfer* transfer,
                             bool signaled);
  void OnWriteComplete(const WriteCallback& callback,
                       PendingHidTransfer* transfer,
                       bool signaled);

  base::win::ScopedHandle file_;

  std::set<scoped_refptr<PendingHidTransfer> > transfers_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_WIN_H_
