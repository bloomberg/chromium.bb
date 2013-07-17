// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_SERVICE_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace content {

class DataFetcherSharedMemory;
class DeviceMotionProvider;
class RenderProcessHost;

// Owns the DeviceMotionProvider (the background polling thread) and keeps
// track of the number of consumers currently using the data (and pausing
// the provider when not in use).
class CONTENT_EXPORT DeviceMotionService {
 public:
  // Returns the DeviceMotionService singleton.
  static DeviceMotionService* GetInstance();

  // Increments the number of users of the provider. The Provider is running
  // when there's > 0 users, and is paused when the count drops to 0.
  //
  // Must be called on the I/O thread.
  void AddConsumer();

  // Removes a consumer. Should be matched with an AddConsumer call.
  //
  // Must be called on the I/O thread.
  void RemoveConsumer();

  // Returns the shared memory handle of the device motion data duplicated
  // into the given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle handle);

  // Stop/join with the background thread in DeviceMotionProvider |provider_|.
  void Shutdown();

 private:
  friend struct DefaultSingletonTraits<DeviceMotionService>;

  DeviceMotionService();
  virtual ~DeviceMotionService();

  int num_readers_;
  bool is_shutdown_;
  scoped_ptr<DeviceMotionProvider> provider_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_SERVICE_H_
