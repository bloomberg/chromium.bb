// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/service/sequence_id.h"

struct GpuChannelMsg_ScheduleImageDecode_Params;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class Message;
}  // namespace IPC

namespace gpu {
class GpuChannel;
class SyncPointClientState;

// Processes incoming image decode requests from renderers: it schedules the
// decode with the appropriate hardware decode accelerator and releases sync
// tokens as decodes complete. These sync tokens must be generated on the client
// side (in ImageDecodeAcceleratorProxy) using the following information:
//
// - The command buffer namespace is GPU_IO.
// - The command buffer ID is created using the
//   CommandBufferIdFromChannelAndRoute() function using
//   GpuChannelReservedRoutes::kImageDecodeAccelerator as the route ID.
// - The release count should be incremented for each decode request.
//
// An object of this class is meant to be used in
// both the IO thread (for receiving decode requests) and the main thread (for
// processing completed decodes).
class ImageDecodeAcceleratorStub
    : public base::RefCountedThreadSafe<ImageDecodeAcceleratorStub> {
 public:
  ImageDecodeAcceleratorStub(GpuChannel* channel, int32_t route_id);

  // Processes a message from the renderer. Should be called on the IO thread.
  bool OnMessageReceived(const IPC::Message& msg);

  // Called on the main thread to indicate that |channel_| should no longer be
  // used.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<ImageDecodeAcceleratorStub>;
  ~ImageDecodeAcceleratorStub();

  void OnScheduleImageDecode(
      const GpuChannelMsg_ScheduleImageDecode_Params& params,
      uint64_t release_count);

  base::Lock lock_;
  GpuChannel* channel_ GUARDED_BY(lock_);
  SequenceId sequence_ GUARDED_BY(lock_);
  scoped_refptr<SyncPointClientState> sync_point_client_state_
      GUARDED_BY(lock_);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeAcceleratorStub);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_
