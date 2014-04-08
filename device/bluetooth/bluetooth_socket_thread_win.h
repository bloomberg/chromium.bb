// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_WIN_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace device {

// Thread abstraction used by |BluetoothSocketWWin| to perform IO operations on
// the underlying platform sockets. An instance of this class can be shared by
// many active sockets.
class BluetoothSocketThreadWin
    : public base::RefCountedThreadSafe<BluetoothSocketThreadWin> {
 public:
  static scoped_refptr<BluetoothSocketThreadWin> Get();
  void OnSocketActivate();
  void OnSocketDeactivate();

  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

 private:
  friend class base::RefCountedThreadSafe<BluetoothSocketThreadWin>;
  BluetoothSocketThreadWin();
  virtual ~BluetoothSocketThreadWin();

  void EnsureStarted();

  base::ThreadChecker thread_checker_;
  int active_socket_count_;
  scoped_ptr<base::Thread> thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketThreadWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_WIN_H_
