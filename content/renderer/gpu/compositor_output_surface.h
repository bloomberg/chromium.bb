// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "cc/output_surface.h"
#include "cc/software_output_device.h"

namespace base {
class TaskRunner;
}

namespace IPC {
class ForwardingMessageFilter;
class Message;
}

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class CompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  static IPC::ForwardingMessageFilter* CreateFilter(
      base::TaskRunner* target_task_runner);

  CompositorOutputSurface(int32 routing_id,
                          WebKit::WebGraphicsContext3D* context3d,
                          cc::SoftwareOutputDevice* software);
  virtual ~CompositorOutputSurface();

  // cc::OutputSurface implementation.
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;
  virtual const struct Capabilities& Capabilities() const OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3D() const OVERRIDE;
  virtual cc::SoftwareOutputDevice* SoftwareDevice() const OVERRIDE;
  virtual void SendFrameToParentCompositor(cc::CompositorFrame*) OVERRIDE;

  // TODO(epenner): This seems out of place here and would be a better fit
  // int CompositorThread after it is fully refactored (http://crbug/170828)
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) OVERRIDE;

 private:
  class CompositorOutputSurfaceProxy :
      public base::RefCountedThreadSafe<CompositorOutputSurfaceProxy> {
   public:
    explicit CompositorOutputSurfaceProxy(
        CompositorOutputSurface* output_surface)
        : output_surface_(output_surface) {}
    void ClearOutputSurface() { output_surface_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (output_surface_)
        output_surface_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<CompositorOutputSurfaceProxy>;
    ~CompositorOutputSurfaceProxy() {}
    CompositorOutputSurface* output_surface_;

    DISALLOW_COPY_AND_ASSIGN(CompositorOutputSurfaceProxy);
  };

  void OnMessageReceived(const IPC::Message& message);
  void OnUpdateVSyncParameters(
      base::TimeTicks timebase, base::TimeDelta interval);
  bool Send(IPC::Message* message);

  scoped_refptr<IPC::ForwardingMessageFilter> output_surface_filter_;
  cc::OutputSurfaceClient* client_;
  scoped_refptr<CompositorOutputSurfaceProxy> output_surface_proxy_;
  int routing_id_;
  struct Capabilities capabilities_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3D_;
  scoped_ptr<cc::SoftwareOutputDevice> software_device_;
  bool prefers_smoothness_;
  base::PlatformThreadId main_thread_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
