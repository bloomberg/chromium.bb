// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer.h"

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "ui/gfx/transform.h"

using content::BrowserThread;

namespace android_webview {

namespace {

// Used to calculate memory and resource allocation. Determined experimentally.
const size_t g_memory_multiplier = 10;
const size_t g_num_gralloc_limit = 150;
const size_t kBytesPerPixel = 4;
const size_t kMemoryAllocationStep = 5 * 1024 * 1024;

base::LazyInstance<scoped_refptr<internal::DeferredGpuCommandService> >
    g_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

HardwareRenderer::HardwareRenderer(SharedRendererState* state)
    : shared_renderer_state_(state),
      last_egl_context_(eglGetCurrentContext()),
      manager_key_(GLViewRendererManager::GetInstance()->PushBack(
          shared_renderer_state_)) {
  DCHECK(last_egl_context_);
  if (!g_service.Get()) {
    g_service.Get() = new internal::DeferredGpuCommandService;
    content::SynchronousCompositor::SetGpuService(g_service.Get());
  }

  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
  internal::ScopedAllowGL allow_gl;

  gl_surface_ = new AwGLSurface;
  bool success =
      shared_renderer_state_->GetCompositor()->
          InitializeHwDraw(gl_surface_);
  DCHECK(success);
}

HardwareRenderer::~HardwareRenderer() {
  GLViewRendererManager* mru = GLViewRendererManager::GetInstance();
  mru->Remove(manager_key_);

  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
  internal::ScopedAllowGL allow_gl;

  shared_renderer_state_->GetCompositor()->ReleaseHwDraw();
  gl_surface_ = NULL;
}

bool HardwareRenderer::DrawGL(AwDrawGLInfo* draw_info, DrawGLResult* result) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::DrawGL");
  GLViewRendererManager::GetInstance()->DidDrawGL(manager_key_);
  const DrawGLInput input = shared_renderer_state_->GetDrawGLInput();

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  if (!current_context) {
    DLOG(ERROR) << "DrawGL called without EGLContext";
    return false;
  }

  // TODO(boliu): Handle context loss.
  if (last_egl_context_ != current_context)
    DLOG(WARNING) << "EGLContextChanged";

  ScopedAppGLStateRestore state_restore(ScopedAppGLStateRestore::MODE_DRAW);
  internal::ScopedAllowGL allow_gl;

  if (draw_info->mode == AwDrawGLInfo::kModeProcess)
    return false;

  // Update memory budget. This will no-op in compositor if the policy has not
  // changed since last draw.
  content::SynchronousCompositorMemoryPolicy policy;
  policy.bytes_limit = g_memory_multiplier * kBytesPerPixel *
                       input.global_visible_rect.width() *
                       input.global_visible_rect.height();
  // Round up to a multiple of kMemoryAllocationStep.
  policy.bytes_limit =
      (policy.bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;
  policy.num_resources_limit = g_num_gralloc_limit;
  SetMemoryPolicy(policy);

  gl_surface_->SetBackingFrameBufferObject(
      state_restore.framebuffer_binding_ext());

  gfx::Transform transform;
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(input.scroll_offset.x(), input.scroll_offset.y());
  gfx::Rect clip_rect(draw_info->clip_left,
                      draw_info->clip_top,
                      draw_info->clip_right - draw_info->clip_left,
                      draw_info->clip_bottom - draw_info->clip_top);

  bool did_draw = shared_renderer_state_->GetCompositor()->
      DemandDrawHw(
          gfx::Size(draw_info->width, draw_info->height),
          transform,
          clip_rect,  // viewport
          clip_rect,
          state_restore.stencil_enabled());
  gl_surface_->ResetBackingFrameBufferObject();

  if (did_draw) {
    result->frame_id = input.frame_id;
    result->clip_contains_visible_rect =
        clip_rect.Contains(input.global_visible_rect);
  }
  return did_draw;
}

bool HardwareRenderer::TrimMemory(int level, bool visible) {
  // Constants from Android ComponentCallbacks2.
  enum {
    TRIM_MEMORY_RUNNING_LOW = 10,
    TRIM_MEMORY_UI_HIDDEN = 20,
    TRIM_MEMORY_BACKGROUND = 40,
  };

  // Not urgent enough. TRIM_MEMORY_UI_HIDDEN is treated specially because
  // it does not indicate memory pressure, but merely that the app is
  // backgrounded.
  if (level < TRIM_MEMORY_RUNNING_LOW || level == TRIM_MEMORY_UI_HIDDEN)
    return false;

  // Do not release resources on view we expect to get DrawGL soon.
  if (level < TRIM_MEMORY_BACKGROUND && visible)
    return false;

  if (!eglGetCurrentContext()) {
    NOTREACHED();
    return false;
  }

  DCHECK_EQ(last_egl_context_, eglGetCurrentContext());

  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  content::SynchronousCompositorMemoryPolicy policy;
  policy.bytes_limit = 0;
  policy.num_resources_limit = 0;
  if (memory_policy_ == policy)
    return false;

  TRACE_EVENT0("android_webview", "HardwareRenderer::TrimMemory");
  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
  internal::ScopedAllowGL allow_gl;

  SetMemoryPolicy(policy);
  return true;
}

void HardwareRenderer::SetMemoryPolicy(
    content::SynchronousCompositorMemoryPolicy& new_policy) {
  if (memory_policy_ == new_policy)
    return;

  memory_policy_ = new_policy;
  shared_renderer_state_->GetCompositor()->
      SetMemoryPolicy(memory_policy_);
}

// static
void HardwareRenderer::CalculateTileMemoryPolicy() {
  CommandLine* cl = CommandLine::ForCurrentProcess();

  const char kDefaultTileSize[] = "384";
  if (!cl->HasSwitch(switches::kDefaultTileWidth))
    cl->AppendSwitchASCII(switches::kDefaultTileWidth, kDefaultTileSize);

  if (!cl->HasSwitch(switches::kDefaultTileHeight))
    cl->AppendSwitchASCII(switches::kDefaultTileHeight, kDefaultTileSize);
}

namespace internal {

bool ScopedAllowGL::allow_gl = false;

// static
bool ScopedAllowGL::IsAllowed() {
  return GLViewRendererManager::GetInstance()->OnRenderThread() && allow_gl;
}

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(GLViewRendererManager::GetInstance()->OnRenderThread());
  DCHECK(!allow_gl);
  allow_gl = true;

  if (g_service.Get())
    g_service.Get()->RunTasks();
}

ScopedAllowGL::~ScopedAllowGL() { allow_gl = false; }

DeferredGpuCommandService::DeferredGpuCommandService() {}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

// static
void DeferredGpuCommandService::RequestProcessGLOnUIThread() {
  SharedRendererState* renderer_state =
      GLViewRendererManager::GetInstance()->GetMostRecentlyDrawn();
  if (!renderer_state) {
    LOG(ERROR) << "No hardware renderer. Deadlock likely";
    return;
  }
  renderer_state->ClientRequestDrawGL();
}

// Called from different threads!
void DeferredGpuCommandService::ScheduleTask(const base::Closure& task) {
  {
    base::AutoLock lock(tasks_lock_);
    tasks_.push(task);
  }
  if (ScopedAllowGL::IsAllowed()) {
    RunTasks();
  } else {
    RequestProcessGLOnUIThread();
  }
}

void DeferredGpuCommandService::ScheduleIdleWork(
    const base::Closure& callback) {
  // TODO(sievers): Should this do anything?
}

bool DeferredGpuCommandService::UseVirtualizedGLContexts() { return true; }

scoped_refptr<gpu::gles2::ShaderTranslatorCache>
DeferredGpuCommandService::shader_translator_cache() {
  if (!shader_translator_cache_.get())
    shader_translator_cache_ = new gpu::gles2::ShaderTranslatorCache;
  return shader_translator_cache_;
}

void DeferredGpuCommandService::RunTasks() {
  bool has_more_tasks;
  {
    base::AutoLock lock(tasks_lock_);
    has_more_tasks = tasks_.size() > 0;
  }

  while (has_more_tasks) {
    base::Closure task;
    {
      base::AutoLock lock(tasks_lock_);
      task = tasks_.front();
      tasks_.pop();
    }
    task.Run();
    {
      base::AutoLock lock(tasks_lock_);
      has_more_tasks = tasks_.size() > 0;
    }
  }
}

void DeferredGpuCommandService::AddRef() const {
  base::RefCountedThreadSafe<DeferredGpuCommandService>::AddRef();
}

void DeferredGpuCommandService::Release() const {
  base::RefCountedThreadSafe<DeferredGpuCommandService>::Release();
}

}  // namespace internal

}  // namespace android_webview
