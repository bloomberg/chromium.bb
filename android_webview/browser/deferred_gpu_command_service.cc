// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/deferred_gpu_command_service.h"

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/render_thread_manager.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_switches.h"

namespace android_webview {

namespace {
base::LazyInstance<scoped_refptr<DeferredGpuCommandService> >
    g_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

base::LazyInstance<base::ThreadLocalBoolean> ScopedAllowGL::allow_gl;

// static
bool ScopedAllowGL::IsAllowed() {
  return allow_gl.Get().Get();
}

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(!allow_gl.Get().Get());
  allow_gl.Get().Set(true);

  if (g_service.Get().get())
    g_service.Get()->RunTasks();
}

ScopedAllowGL::~ScopedAllowGL() {
  allow_gl.Get().Set(false);

  DeferredGpuCommandService* service = g_service.Get().get();
  if (service) {
    service->RunTasks();
    if (service->IdleQueueSize()) {
      service->RequestProcessGL(true);
    }
  }
}

// static
void DeferredGpuCommandService::SetInstance() {
  if (!g_service.Get().get()) {
    g_service.Get() = new DeferredGpuCommandService;
    content::SynchronousCompositor::SetGpuService(g_service.Get());
  }
}

// static
DeferredGpuCommandService* DeferredGpuCommandService::GetInstance() {
  DCHECK(g_service.Get().get());
  return g_service.Get().get();
}

DeferredGpuCommandService::DeferredGpuCommandService()
    : gpu::InProcessCommandBuffer::Service(
          content::GetGpuPreferencesFromCommandLine()),
      sync_point_manager_(new gpu::SyncPointManager(true)) {
}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

// This method can be called on any thread.
// static
void DeferredGpuCommandService::RequestProcessGL(bool for_idle) {
  RenderThreadManager* renderer_state =
      GLViewRendererManager::GetInstance()->GetMostRecentlyDrawn();
  if (!renderer_state) {
    LOG(ERROR) << "No hardware renderer. Deadlock likely";
    return;
  }
  renderer_state->ClientRequestInvokeGL(for_idle);
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
    RequestProcessGL(false);
  }
}

size_t DeferredGpuCommandService::IdleQueueSize() {
  base::AutoLock lock(tasks_lock_);
  return idle_tasks_.size();
}

void DeferredGpuCommandService::ScheduleDelayedWork(
    const base::Closure& callback) {
  {
    base::AutoLock lock(tasks_lock_);
    idle_tasks_.push(std::make_pair(base::Time::Now(), callback));
  }
  RequestProcessGL(true);
}

void DeferredGpuCommandService::PerformIdleWork(bool is_idle) {
  TRACE_EVENT1("android_webview",
               "DeferredGpuCommandService::PerformIdleWork",
               "is_idle",
               is_idle);
  DCHECK(ScopedAllowGL::IsAllowed());
  static const base::TimeDelta kMaxIdleAge =
      base::TimeDelta::FromMilliseconds(16);

  const base::Time now = base::Time::Now();
  size_t queue_size = IdleQueueSize();
  while (queue_size--) {
    base::Closure task;
    {
      base::AutoLock lock(tasks_lock_);
      if (!is_idle) {
        // Only run old tasks if we are not really idle right now.
        base::TimeDelta age(now - idle_tasks_.front().first);
        if (age < kMaxIdleAge)
          break;
      }
      task = idle_tasks_.front().second;
      idle_tasks_.pop();
    }
    task.Run();
  }
}

void DeferredGpuCommandService::PerformAllIdleWork() {
  TRACE_EVENT0("android_webview",
               "DeferredGpuCommandService::PerformAllIdleWork");
  while (IdleQueueSize()) {
    PerformIdleWork(true);
  }
}

bool DeferredGpuCommandService::UseVirtualizedGLContexts() { return true; }

scoped_refptr<gpu::gles2::ShaderTranslatorCache>
DeferredGpuCommandService::shader_translator_cache() {
  if (!shader_translator_cache_.get()) {
    shader_translator_cache_ =
        new gpu::gles2::ShaderTranslatorCache(gpu_preferences());
  }
  return shader_translator_cache_;
}

scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
DeferredGpuCommandService::framebuffer_completeness_cache() {
  if (!framebuffer_completeness_cache_.get()) {
    framebuffer_completeness_cache_ =
        new gpu::gles2::FramebufferCompletenessCache;
  }
  return framebuffer_completeness_cache_;
}

gpu::SyncPointManager* DeferredGpuCommandService::sync_point_manager() {
  return sync_point_manager_.get();
}

void DeferredGpuCommandService::RunTasks() {
  TRACE_EVENT0("android_webview", "DeferredGpuCommandService::RunTasks");
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

}  // namespace android_webview
