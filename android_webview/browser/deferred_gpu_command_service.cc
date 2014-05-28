// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/deferred_gpu_command_service.h"

#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"

namespace android_webview {

namespace {

// TODO(boliu): Consider using base/atomicops.h.
class ThreadSafeBool {
 public:
  ThreadSafeBool();
  void Set(bool boolean);
  bool Get();

 private:
  base::Lock lock_;
  bool boolean_;
  DISALLOW_COPY_AND_ASSIGN(ThreadSafeBool);
};

ThreadSafeBool::ThreadSafeBool() : boolean_(false) {
}

void ThreadSafeBool::Set(bool boolean) {
  base::AutoLock lock(lock_);
  boolean_ = boolean;
}

bool ThreadSafeBool::Get() {
  base::AutoLock lock(lock_);
  return boolean_;
}

base::LazyInstance<ThreadSafeBool> g_request_pending =
    LAZY_INSTANCE_INITIALIZER;

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

  if (g_service.Get())
    g_service.Get()->RunTasks();
}

ScopedAllowGL::~ScopedAllowGL() {
  allow_gl.Get().Set(false);
  g_request_pending.Get().Set(false);

  if (g_service.Get())
    g_service.Get()->RunTasks();
}

// static
void DeferredGpuCommandService::SetInstance() {
  if (!g_service.Get()) {
    g_service.Get() = new DeferredGpuCommandService;
    content::SynchronousCompositor::SetGpuService(g_service.Get());

    // Initialize global booleans.
    g_request_pending.Get().Set(false);
  }
}

// static
DeferredGpuCommandService* DeferredGpuCommandService::GetInstance() {
  DCHECK(g_service.Get().get());
  return g_service.Get().get();
}

DeferredGpuCommandService::DeferredGpuCommandService() {}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

// This method can be called on any thread.
// static
void DeferredGpuCommandService::RequestProcessGL() {
  SharedRendererState* renderer_state =
      GLViewRendererManager::GetInstance()->GetMostRecentlyDrawn();
  if (!renderer_state) {
    LOG(ERROR) << "No hardware renderer. Deadlock likely";
    return;
  }

  if (!g_request_pending.Get().Get()) {
    g_request_pending.Get().Set(true);
    renderer_state->ClientRequestDrawGL();
  }
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
    RequestProcessGL();
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

}  // namespace android_webview
