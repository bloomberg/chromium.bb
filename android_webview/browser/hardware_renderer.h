// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include <queue>

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/android/synchronous_compositor.h"

struct AwDrawGLInfo;

namespace android_webview {

class AwGLSurface;
class BrowserViewRendererClient;

namespace internal {
class DeferredGpuCommandService;
}  // namespace internal

struct DrawGLInput {
  gfx::Rect global_visible_rect;
  gfx::Vector2d scroll;
};

struct DrawGLResult {
  bool did_draw;
  bool clip_contains_visible_rect;

  DrawGLResult();
};

class HardwareRenderer {
 public:
  HardwareRenderer(content::SynchronousCompositor* compositor,
                   BrowserViewRendererClient* client);
  ~HardwareRenderer();

  static void CalculateTileMemoryPolicy();

  DrawGLResult DrawGL(AwDrawGLInfo* draw_info, const DrawGLInput& input);
  bool TrimMemory(int level, bool visible);
  void SetMemoryPolicy(content::SynchronousCompositorMemoryPolicy& new_policy);

  bool RequestDrawGL();

 private:
  friend class internal::DeferredGpuCommandService;

  bool InitializeHardwareDraw();

  content::SynchronousCompositor* compositor_;
  BrowserViewRendererClient* client_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  scoped_refptr<AwGLSurface> gl_surface_;
  content::SynchronousCompositorMemoryPolicy memory_policy_;

  GLViewRendererManager::Key manager_key_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

namespace internal {

class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed();

 private:
  static bool allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

class DeferredGpuCommandService
    : public gpu::InProcessCommandBuffer::Service,
      public base::RefCountedThreadSafe<DeferredGpuCommandService> {
 public:
  DeferredGpuCommandService();

  virtual void ScheduleTask(const base::Closure& task) OVERRIDE;
  virtual void ScheduleIdleWork(const base::Closure& task) OVERRIDE;
  virtual bool UseVirtualizedGLContexts() OVERRIDE;

  void RunTasks();

  virtual void AddRef() const OVERRIDE;
  virtual void Release() const OVERRIDE;

 protected:
  virtual ~DeferredGpuCommandService();
  friend class base::RefCountedThreadSafe<DeferredGpuCommandService>;

 private:
  static void RequestProcessGLOnUIThread();

  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
