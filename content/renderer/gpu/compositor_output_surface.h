// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSoftwareOutputDevice.h"

namespace base {
  class TaskRunner;
}

namespace IPC {
  class ForwardingMessageFilter;
  class Message;
  class SyncChannel;
}

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class CompositorOutputSurface
    : NON_EXPORTED_BASE(public WebKit::WebCompositorOutputSurface)
    , NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  static IPC::ForwardingMessageFilter* CreateFilter(
      base::TaskRunner* target_task_runner);

  CompositorOutputSurface(int32 routing_id,
                          WebKit::WebGraphicsContext3D* context3d,
                          WebKit::WebCompositorSoftwareOutputDevice* software);
  virtual ~CompositorOutputSurface();

  // WebCompositorOutputSurface implementation.
  virtual bool bindToClient(
      WebKit::WebCompositorOutputSurfaceClient* client) OVERRIDE;
  virtual const Capabilities& capabilities() const OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* context3D() const OVERRIDE;
  virtual WebKit::WebCompositorSoftwareOutputDevice* softwareDevice() const;
  virtual void sendFrameToParentCompositor(
      const WebKit::WebCompositorFrame&) OVERRIDE;

 private:
  void OnMessageReceived(const IPC::Message& message);
  void OnUpdateVSyncParameters(
      base::TimeTicks timebase, base::TimeDelta interval);

  scoped_refptr<IPC::ForwardingMessageFilter> output_surface_filter_;
  WebKit::WebCompositorOutputSurfaceClient* client_;
  int routing_id_;
  Capabilities capabilities_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3D_;
  scoped_ptr<WebKit::WebCompositorSoftwareOutputDevice> software_device_;
};

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
