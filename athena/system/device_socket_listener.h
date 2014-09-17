// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_DEVICE_SOCKET_LISTENER_H_
#define ATHENA_SYSTEM_DEVICE_SOCKET_LISTENER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace base {
class TaskRunner;
}

namespace athena {

// This class reads device-data from a socket.
class DeviceSocketListener {
 public:
  DeviceSocketListener(const std::string& socket_parth,
                       size_t data_size);
  virtual ~DeviceSocketListener();

  static void CreateSocketManager(
      scoped_refptr<base::TaskRunner> io_task_runner);
  static void ShutdownSocketManager();

  // This is called on the FILE thread when data is available on the socket.
  // |data| is guaranteed to be of exactly |data_size| length. Note that the
  // implementation must not mutate or free the data.
  virtual void OnDataAvailableOnFILE(const void* data) = 0;

 protected:
  void StartListening();
  void StopListening();

 private:
  const std::string socket_path_;
  size_t data_size_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSocketListener);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_DEVICE_SOCKET_LISTENER_H_
