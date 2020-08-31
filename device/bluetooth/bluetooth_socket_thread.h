// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace device {

// Thread abstraction used by |BluetoothSocketBlueZ| and |BluetoothSocketWin|
// to perform IO operations on the underlying platform sockets. An instance of
// this class can be shared by many active sockets.
class DEVICE_BLUETOOTH_EXPORT BluetoothSocketThread
    : public base::RefCountedThreadSafe<BluetoothSocketThread> {
 public:
  static scoped_refptr<BluetoothSocketThread> Get();
  static void CleanupForTesting();

  void OnSocketActivate();
  void OnSocketDeactivate();

  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

 private:
  friend class base::RefCountedThreadSafe<BluetoothSocketThread>;
  BluetoothSocketThread();
  virtual ~BluetoothSocketThread();

  void EnsureStarted();

  base::ThreadChecker thread_checker_;
  int active_socket_count_;
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketThread);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_
