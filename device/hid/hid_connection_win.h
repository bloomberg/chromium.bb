// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_WIN_H_
#define DEVICE_HID_HID_CONNECTION_WIN_H_

#include <set>
#include <windows.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "base/win/object_watcher.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

namespace device {

class HidConnectionWin : public HidConnection {
 public:
  struct PendingTransfer : public base::RefCounted<PendingTransfer>,
                           public base::win::ObjectWatcher::Delegate,
                           public base::MessageLoop::DestructionObserver {
   public:
    PendingTransfer(scoped_refptr<HidConnectionWin> conn,
                    scoped_refptr<net::IOBuffer> target,
                    scoped_refptr<net::IOBuffer> receiving,
                    bool is_input,
                    IOCallback callback);

    void TakeResultFromWindowsAPI(BOOL result);

    OVERLAPPED* GetOverlapped() { return &overlapped_; }

    // Implements base::win::ObjectWatcher::Delegate.
    virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

    // Implements base::MessageLoop::DestructionObserver
    virtual void WillDestroyCurrentMessageLoop() OVERRIDE;


   private:
    friend class base::RefCounted<PendingTransfer>;
    friend class HidConnectionWin;

    virtual ~PendingTransfer();

    scoped_refptr<HidConnectionWin> conn_;
    bool is_input_;
    scoped_refptr<net::IOBuffer> target_;
    scoped_refptr<net::IOBuffer> receiving_;
    IOCallback callback_;
    OVERLAPPED overlapped_;
    base::win::ScopedHandle event_;
    base::win::ObjectWatcher watcher_;

    DISALLOW_COPY_AND_ASSIGN(PendingTransfer);
  };

  HidConnectionWin(HidDeviceInfo device_info);

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

  void OnTransferFinished(scoped_refptr<PendingTransfer> transfer);
  void OnTransferCanceled(scoped_refptr<PendingTransfer> transfer);

  bool available() const { return available_; }

 private:
  ~HidConnectionWin();

  base::win::ScopedHandle file_;
  std::set<scoped_refptr<PendingTransfer> > transfers_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionWin);

  bool available_;
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_WIN_H_
