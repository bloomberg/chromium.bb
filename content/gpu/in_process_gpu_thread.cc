// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/in_process_gpu_thread.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {

InProcessGpuThread::InProcessGpuThread(
    const InProcessChildThreadParams& params,
    gpu::SyncPointManager* sync_point_manager_override)
    : base::Thread("Chrome_InProcGpuThread"),
      params_(params),
      gpu_process_(NULL),
      sync_point_manager_override_(sync_point_manager_override),
      gpu_memory_buffer_factory_(
          GpuMemoryBufferFactory::GetNativeType() != gfx::EMPTY_BUFFER
              ? GpuMemoryBufferFactory::CreateNativeType()
              : nullptr) {
  if (!sync_point_manager_override_) {
    sync_point_manager_.reset(new gpu::SyncPointManager(false));
    sync_point_manager_override_ = sync_point_manager_.get();
  }
}

InProcessGpuThread::~InProcessGpuThread() {
  Stop();
}

void InProcessGpuThread::Init() {
  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;

#if defined(OS_ANDROID)
  // Call AttachCurrentThreadWithName, before any other AttachCurrentThread()
  // calls. The latter causes Java VM to assign Thread-??? to the thread name.
  // Please note calls to AttachCurrentThreadWithName after AttachCurrentThread
  // will not change the thread name kept in Java VM.
  base::android::AttachCurrentThreadWithName(thread_name());
  // Up the priority of the |io_thread_| on Android.
  io_thread_priority = base::ThreadPriority::DISPLAY;
#endif

  gpu_process_ = new GpuProcess(io_thread_priority);

  // The process object takes ownership of the thread object, so do not
  // save and delete the pointer.
  GpuChildThread* child_thread = new GpuChildThread(
      params_, gpu_memory_buffer_factory_.get(), sync_point_manager_override_);

  // Since we are in the browser process, use the thread start time as the
  // process start time.
  child_thread->Init(base::Time::Now());

  gpu_process_->set_main_thread(child_thread);
}

void InProcessGpuThread::CleanUp() {
  SetThreadWasQuitProperly(true);
  delete gpu_process_;
}

base::Thread* CreateInProcessGpuThread(
    const InProcessChildThreadParams& params) {
  return new InProcessGpuThread(params, nullptr);
}

}  // namespace content
