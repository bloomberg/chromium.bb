// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDER_THREAD_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_RENDER_THREAD_MANAGER_H_

#include <map>

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "cc/output/compositor_frame_ack.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

struct AwDrawGLInfo;
namespace android_webview {

namespace internal {
class RequestInvokeGLTracker;
}

class RenderThreadManagerClient;
class ChildFrame;
class HardwareRenderer;
class InsideHardwareReleaseReset;

// This class is used to pass data between UI thread and RenderThread.
class RenderThreadManager {
 public:
  struct ReturnedResources {
    ReturnedResources();
    ~ReturnedResources();

    uint32_t output_surface_id;
    cc::ReturnedResourceArray resources;
  };
  using ReturnedResourcesMap = std::map<uint32_t, ReturnedResources>;

  RenderThreadManager(
      RenderThreadManagerClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop);
  ~RenderThreadManager();

  // This function can be called from any thread.
  void ClientRequestInvokeGL(bool for_idle);

  // UI thread methods.
  void SetScrollOffsetOnUI(gfx::Vector2d scroll_offset);
  void SetFrameOnUI(std::unique_ptr<ChildFrame> frame);
  void InitializeHardwareDrawIfNeededOnUI();
  ParentCompositorDrawConstraints GetParentDrawConstraintsOnUI() const;
  void SwapReturnedResourcesOnUI(ReturnedResourcesMap* returned_resource_map);
  bool ReturnedResourcesEmptyOnUI() const;
  std::unique_ptr<ChildFrame> PassUncommittedFrameOnUI();
  bool HasFrameOnUI() const;
  void DeleteHardwareRendererOnUI();

  // RT thread methods.
  gfx::Vector2d GetScrollOffsetOnRT();
  std::unique_ptr<ChildFrame> PassFrameOnRT();
  void DrawGL(AwDrawGLInfo* draw_info);
  void PostExternalDrawConstraintsToChildCompositorOnRT(
      const ParentCompositorDrawConstraints& parent_draw_constraints);
  void InsertReturnedResourcesOnRT(const cc::ReturnedResourceArray& resources,
                                   uint32_t compositor_id,
                                   uint32_t output_surface_id);

 private:
  friend class internal::RequestInvokeGLTracker;
  class InsideHardwareReleaseReset {
   public:
    explicit InsideHardwareReleaseReset(
        RenderThreadManager* render_thread_manager);
    ~InsideHardwareReleaseReset();

   private:
    RenderThreadManager* render_thread_manager_;
  };

  // RT thread method.
  void DidInvokeGLProcess();
  bool HasFrameForHardwareRendererOnRT() const;

  // UI thread methods.
  void ResetRequestInvokeGLCallback();
  void ClientRequestInvokeGLOnUI();
  void UpdateParentDrawConstraintsOnUI();
  bool IsInsideHardwareRelease() const;
  void SetInsideHardwareRelease(bool inside);

  // Accessed by UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_loop_;
  RenderThreadManagerClient* const client_;
  base::WeakPtr<RenderThreadManager> ui_thread_weak_ptr_;
  base::CancelableClosure request_draw_gl_cancelable_closure_;

  // Accessed by RT thread.
  std::unique_ptr<HardwareRenderer> hardware_renderer_;

  // This is accessed by both UI and RT now. TODO(hush): move to RT only.
  GLViewRendererManager::Key renderer_manager_key_;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  bool hardware_renderer_has_frame_;
  gfx::Vector2d scroll_offset_;
  std::unique_ptr<ChildFrame> child_frame_;
  bool inside_hardware_release_;
  ParentCompositorDrawConstraints parent_draw_constraints_;
  ReturnedResourcesMap returned_resources_map_;
  base::Closure request_draw_gl_closure_;

  base::WeakPtrFactory<RenderThreadManager> weak_factory_on_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDER_THREAD_MANAGER_H_
