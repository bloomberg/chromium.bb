// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
#define ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {
class CompositorFrameAck;
}

namespace gpu {
class GLInProcessContext;
}

namespace android_webview {

class BrowserViewRendererClient;
class InsideHardwareReleaseReset;

// Set by BrowserViewRenderer and read by HardwareRenderer.
struct DrawGLInput {
  gfx::Vector2d scroll_offset;
  int width;
  int height;
  cc::CompositorFrame frame;

  DrawGLInput();
  ~DrawGLInput();
};

// This class is used to pass data between UI thread and RenderThread.
class SharedRendererState {
 public:
  SharedRendererState(scoped_refptr<base::MessageLoopProxy> ui_loop,
                      BrowserViewRendererClient* client);
  ~SharedRendererState();

  void ClientRequestDrawGL();

  void SetDrawGLInput(scoped_ptr<DrawGLInput> input);
  scoped_ptr<DrawGLInput> PassDrawGLInput();

  bool IsInsideHardwareRelease() const;

  void SetSharedContext(gpu::GLInProcessContext* context);
  gpu::GLInProcessContext* GetSharedContext() const;

  void InsertReturnedResources(const cc::ReturnedResourceArray& resources);
  void SwapReturnedResources(cc::ReturnedResourceArray* resources);
  bool ReturnedResourcesEmpty() const;

 private:
  friend class InsideHardwareReleaseReset;

  void ClientRequestDrawGLOnUIThread();
  void SetInsideHardwareRelease(bool inside);

  scoped_refptr<base::MessageLoopProxy> ui_loop_;
  BrowserViewRendererClient* client_on_ui_;
  base::WeakPtrFactory<SharedRendererState> weak_factory_on_ui_thread_;
  base::WeakPtr<SharedRendererState> ui_thread_weak_ptr_;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  scoped_ptr<DrawGLInput> draw_gl_input_;
  bool inside_hardware_release_;
  gpu::GLInProcessContext* share_context_;
  cc::ReturnedResourceArray returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(SharedRendererState);
};

class InsideHardwareReleaseReset {
 public:
  explicit InsideHardwareReleaseReset(
      SharedRendererState* shared_renderer_state);
  ~InsideHardwareReleaseReset();

 private:
  SharedRendererState* shared_renderer_state_;

  DISALLOW_COPY_AND_ASSIGN(InsideHardwareReleaseReset);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
