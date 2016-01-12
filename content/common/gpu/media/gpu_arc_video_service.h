// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_ARC_VIDEO_SERVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_ARC_VIDEO_SERVICE_H_

#include <map>

#include "base/callback.h"
#include "base/threading/thread_checker.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {

// GpuArcVideoService manages life-cycle and IPC message translation for
// ArcVideoAccelerator.
//
// For each creation request from GpuChannelManager, GpuArcVideoService will
// create a new IPC channel.
class GpuArcVideoService {
 public:
  class AcceleratorStub;
  using CreateChannelCallback = base::Callback<void(const IPC::ChannelHandle&)>;

  // |shutdown_event| should signal an event when this process is about to be
  // shut down in order to notify our new IPC channel to terminate.
  GpuArcVideoService(
      base::WaitableEvent* shutdown_event,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  // Upon deletion, all ArcVideoAccelerator will be deleted and the associated
  // IPC channels are closed.
  ~GpuArcVideoService();

  // Creates a new accelerator stub. The creation result will be sent back via
  // |callback|.
  void CreateChannel(const CreateChannelCallback& callback);

  // Removes the reference of |stub| (and trigger deletion) from this class.
  void RemoveClient(AcceleratorStub* stub);

 private:
  base::ThreadChecker thread_checker_;

  // Shutdown event of GPU process.
  base::WaitableEvent* shutdown_event_;

  // GPU io thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Bookkeeping all accelerator stubs.
  std::map<AcceleratorStub*, scoped_ptr<AcceleratorStub>> accelerator_stubs_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoService);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_ARC_VIDEO_SERVICE_H_
