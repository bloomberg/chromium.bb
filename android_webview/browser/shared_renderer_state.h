// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
#define ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "cc/output/compositor_frame_ack.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

struct AwDrawGLInfo;

namespace android_webview {

namespace internal {
class RequestDrawGLTracker;
}

class BrowserViewRenderer;
class ChildFrame;
class HardwareRenderer;
class InsideHardwareReleaseReset;

// This class is used to pass data between UI thread and RenderThread.
class SharedRendererState {
 public:
  SharedRendererState(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop,
      BrowserViewRenderer* browser_view_renderer);
  ~SharedRendererState();

  // This function can be called from any thread.
  void ClientRequestDrawGL();

  // UI thread methods.
  void SetScrollOffsetOnUI(gfx::Vector2d scroll_offset);
  void SetCompositorFrameOnUI(scoped_ptr<ChildFrame> frame);
  void InitializeHardwareDrawIfNeededOnUI();
  void ReleaseHardwareDrawIfNeededOnUI();
  ParentCompositorDrawConstraints GetParentDrawConstraintsOnUI() const;
  void SwapReturnedResourcesOnUI(cc::ReturnedResourceArray* resources);
  bool ReturnedResourcesEmptyOnUI() const;
  scoped_ptr<ChildFrame> PassUncommittedFrameOnUI();

  // RT thread methods.
  gfx::Vector2d GetScrollOffsetOnRT();
  scoped_ptr<ChildFrame> PassCompositorFrameOnRT();
  void DrawGL(AwDrawGLInfo* draw_info);
  void PostExternalDrawConstraintsToChildCompositorOnRT(
      const ParentCompositorDrawConstraints& parent_draw_constraints);
  void InsertReturnedResourcesOnRT(const cc::ReturnedResourceArray& resources);

 private:
  friend class internal::RequestDrawGLTracker;
  class InsideHardwareReleaseReset {
   public:
    explicit InsideHardwareReleaseReset(
        SharedRendererState* shared_renderer_state);
    ~InsideHardwareReleaseReset();

   private:
    SharedRendererState* shared_renderer_state_;
  };

  // RT thread method.
  void DidDrawGLProcess();

  // UI thread methods.
  void ResetRequestDrawGLCallback();
  void ClientRequestDrawGLOnUI();
  void UpdateParentDrawConstraintsOnUI();
  bool IsInsideHardwareRelease() const;
  void SetInsideHardwareRelease(bool inside);

  // Accessed by UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_loop_;
  BrowserViewRenderer* browser_view_renderer_;
  base::WeakPtr<SharedRendererState> ui_thread_weak_ptr_;
  base::CancelableClosure request_draw_gl_cancelable_closure_;

  // Accessed by RT thread.
  scoped_ptr<HardwareRenderer> hardware_renderer_;

  // This is accessed by both UI and RT now. TODO(hush): move to RT only.
  GLViewRendererManager::Key renderer_manager_key_;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  gfx::Vector2d scroll_offset_;
  scoped_ptr<ChildFrame> child_frame_;
  bool inside_hardware_release_;
  ParentCompositorDrawConstraints parent_draw_constraints_;
  cc::ReturnedResourceArray returned_resources_;
  base::Closure request_draw_gl_closure_;

  base::WeakPtrFactory<SharedRendererState> weak_factory_on_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(SharedRendererState);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
