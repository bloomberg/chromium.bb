// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
#define ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_

#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace android_webview {

namespace internal {
class RequestDrawGLTracker;
}

class BrowserViewRendererClient;
class InsideHardwareReleaseReset;

// This class is used to pass data between UI thread and RenderThread.
class SharedRendererState {
 public:
  SharedRendererState(scoped_refptr<base::MessageLoopProxy> ui_loop,
                      BrowserViewRendererClient* client);
  ~SharedRendererState();

  void ClientRequestDrawGL();
  void DidDrawGLProcess();

  void SetScrollOffset(gfx::Vector2d scroll_offset);
  gfx::Vector2d GetScrollOffset();

  bool HasCompositorFrame() const;
  void SetCompositorFrame(scoped_ptr<cc::CompositorFrame> frame,
                          bool force_commit);
  scoped_ptr<cc::CompositorFrame> PassCompositorFrame();
  bool ForceCommit() const;

  bool IsInsideHardwareRelease() const;
  // Returns true if the draw constraints are updated.
  bool UpdateDrawConstraints(
      const ParentCompositorDrawConstraints& parent_draw_constraints);
  void PostExternalDrawConstraintsToChildCompositor(
      const ParentCompositorDrawConstraints& parent_draw_constraints);
  void DidSkipCommitFrame();

  const ParentCompositorDrawConstraints ParentDrawConstraints() const;

  void SetForceInvalidateOnNextDrawGL(
      bool needs_force_invalidate_on_next_draw_gl);
  bool NeedsForceInvalidateOnNextDrawGL() const;

  void InsertReturnedResources(const cc::ReturnedResourceArray& resources);
  void SwapReturnedResources(cc::ReturnedResourceArray* resources);
  bool ReturnedResourcesEmpty() const;

 private:
  friend class InsideHardwareReleaseReset;
  friend class internal::RequestDrawGLTracker;

  void ResetRequestDrawGLCallback();
  void ClientRequestDrawGLOnUIThread();
  void UpdateParentDrawConstraintsOnUIThread();
  void DidSkipCommitFrameOnUIThread();
  void SetInsideHardwareRelease(bool inside);

  scoped_refptr<base::MessageLoopProxy> ui_loop_;
  BrowserViewRendererClient* client_on_ui_;
  base::WeakPtr<SharedRendererState> ui_thread_weak_ptr_;
  base::CancelableClosure request_draw_gl_cancelable_closure_;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  gfx::Vector2d scroll_offset_;
  scoped_ptr<cc::CompositorFrame> compositor_frame_;
  bool force_commit_;
  bool inside_hardware_release_;
  bool needs_force_invalidate_on_next_draw_gl_;
  ParentCompositorDrawConstraints parent_draw_constraints_;
  cc::ReturnedResourceArray returned_resources_;
  base::Closure request_draw_gl_closure_;

  base::WeakPtrFactory<SharedRendererState> weak_factory_on_ui_thread_;

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
