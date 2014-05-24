// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_

#include <queue>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_local.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"

namespace android_webview {

class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed();

 private:
  static base::LazyInstance<base::ThreadLocalBoolean> allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

class DeferredGpuCommandService
    : public gpu::InProcessCommandBuffer::Service,
      public base::RefCountedThreadSafe<DeferredGpuCommandService> {
 public:
  static void SetInstance();
  static DeferredGpuCommandService* GetInstance();

  virtual void ScheduleTask(const base::Closure& task) OVERRIDE;
  virtual void ScheduleIdleWork(const base::Closure& task) OVERRIDE;
  virtual bool UseVirtualizedGLContexts() OVERRIDE;
  virtual scoped_refptr<gpu::gles2::ShaderTranslatorCache>
      shader_translator_cache() OVERRIDE;

  void RunTasks();

  virtual void AddRef() const OVERRIDE;
  virtual void Release() const OVERRIDE;

 protected:
  virtual ~DeferredGpuCommandService();
  friend class base::RefCountedThreadSafe<DeferredGpuCommandService>;

 private:
  static void RequestProcessGL();

  DeferredGpuCommandService();

  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;

  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_
