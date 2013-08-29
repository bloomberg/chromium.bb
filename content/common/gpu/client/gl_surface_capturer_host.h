// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_SURFACE_CAPTURER_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_SURFACE_CAPTURER_HOST_H_

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/surface_capturer.h"
#include "ipc/ipc_listener.h"

namespace gfx {

class Rect;
class Size;

}  // namespace gfx

namespace media {

class VideoFrame;

}  // namespace media

namespace content {

class GpuChannelHost;

// This class is the browser-side host for the SurfaceCapturer in the GPU
// process, coordinated over IPC.
class GLSurfaceCapturerHost : public IPC::Listener,
                              public SurfaceCapturer,
                              public CommandBufferProxyImpl::DeletionObserver {
 public:
  // |client| is expected to outlive this object.  We are registered as a
  // DeletionObserver with |impl|, so it will notify us when it is about to
  // be destroyed.
  GLSurfaceCapturerHost(int32 capturer_route_id,
                        SurfaceCapturer::Client* client,
                        CommandBufferProxyImpl* impl);

  // IPC::Listener implementation.
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // SurfaceCapturer implementation.
  virtual void Initialize(media::VideoFrame::Format format) OVERRIDE;
  virtual void TryCapture() OVERRIDE;
  virtual void CopyCaptureToVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // CommandBufferProxyImpl::DeletionObserver implementation.
  virtual void OnWillDeleteImpl() OVERRIDE;

 private:
  virtual ~GLSurfaceCapturerHost();

  // Notify |client_| when an error has occured.  Used when notifying from
  // within a media::VideoEncodeAccelerator entry point, to avoid re-entrancy.
  void NotifyError(Error error);

  // IPC handlers, proxying SurfaceCapturer::Client for the GPU process.
  void OnNotifyCaptureParameters(const gfx::Size& buffer_size,
                                 const gfx::Rect& visible_rect);
  void OnNotifyCopyCaptureDone(int32 frame_id);
  void OnNotifyError(Error error);

  void Send(IPC::Message* message);

  const base::ThreadChecker thread_checker_;

  // Route ID for corresponding GLSurfaceCapturer in the GPU process.
  const int32 capturer_route_id_;

  // Weak pointer for registering as a listener for |channel_|.
  base::WeakPtrFactory<GLSurfaceCapturerHost> weak_this_factory_;

  // SurfaceEncoder::Client callbacks received over IPC are forwarded to
  // |client_|.
  SurfaceCapturer::Client* client_;

  // Unowned reference to the CommandBufferProxyImpl that created us.
  CommandBufferProxyImpl* impl_;

  // GPU process channel to send messages on.
  GpuChannelHost* channel_;

  // media::VideoFrames sent to the capturer.
  // base::IDMap not used here, since that takes pointers, not scoped_refptr.
  typedef base::SmallMap<std::map<int32, scoped_refptr<media::VideoFrame> > >
      FrameMap;
  FrameMap frame_map_;

  // ID serial number for next frame to send to the GPU process.
  int32 next_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceCapturerHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_SURFACE_CAPTURER_HOST_H_
