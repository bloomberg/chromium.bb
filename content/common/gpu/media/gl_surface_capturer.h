// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GL_SURFACE_CAPTURER_H_
#define CONTENT_COMMON_GPU_MEDIA_GL_SURFACE_CAPTURER_H_

#include "base/callback.h"
#include "base/containers/small_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/surface_capturer.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace gfx {

class Size;
class Rect;

}  // namespace gfx

namespace content {

class GpuChannelHost;

// This class implements the GPU process side of a SurfaceCapturer,
// communicating over IPC with a GLSurfaceCapturerHost on the browser process.
class GLSurfaceCapturer : public IPC::Listener,
                          public SurfaceCapturer::Client,
                          public GpuCommandBufferStub::DestructionObserver {
 public:
  GLSurfaceCapturer(int32 route_id, GpuCommandBufferStub* stub);
  virtual ~GLSurfaceCapturer();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // SurfaceCapturer::Client implementation.
  virtual void NotifyCaptureParameters(const gfx::Size& buffer_size,
                                       const gfx::Rect& visible_rect) OVERRIDE;
  virtual void NotifyCopyCaptureDone(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;
  virtual void NotifyError(SurfaceCapturer::Error error) OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub() OVERRIDE;

 private:
  // Handlers for IPC messages.
  void OnInitialize(media::VideoFrame::Format format);
  void OnCopyCaptureToVideoFrame(int32 frame_id,
                                 base::SharedMemoryHandle buffer_shm,
                                 uint32 buffer_size);
  void OnTryCapture();
  void OnDestroy();

  void Send(IPC::Message* message);

  const base::ThreadChecker thread_checker_;

  // Route ID assigned by the GpuCommandBufferStub.
  const int32 route_id_;

  // The GpuCommandBufferStub this instance belongs to, as an unowned pointer
  // since |stub_| will own (and outlive) this.
  GpuCommandBufferStub* const stub_;

  // media::VideoFrames received from the host side.
  typedef base::SmallMap<std::map<scoped_refptr<media::VideoFrame>, int32> >
      FrameIdMap;
  FrameIdMap frame_id_map_;

  // The underlying capturer we delegate to.
  scoped_ptr<SurfaceCapturer> surface_capturer_;

  // The capture output parameters that the capturer informs us of.
  media::VideoFrame::Format output_format_;
  gfx::Size output_buffer_size_;
  gfx::Rect output_visible_rect_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceCapturer);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GL_SURFACE_CAPTURER_H_
