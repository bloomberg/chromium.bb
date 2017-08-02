// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/in_process_gpu_thread.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

InProcessGpuThread::InProcessGpuThread(const InProcessChildThreadParams& params)
    : base::Thread("Chrome_InProcGpuThread"),
      params_(params),
      gpu_process_(NULL) {}

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

#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  gpu::GPUInfo gpu_info;
  if (!gl::init::InitializeGLOneOff()) {
    VLOG(1) << "gl::init::InitializeGLOneOff failed";
  } else {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSkipGpuDataLoading))
      gpu::CollectContextGraphicsInfo(&gpu_info);
  }

  gpu::GpuFeatureInfo gpu_feature_info =
      gpu::GetGpuFeatureInfo(gpu_info, *base::CommandLine::ForCurrentProcess());

  // The process object takes ownership of the thread object, so do not
  // save and delete the pointer.
  GpuChildThread* child_thread =
      new GpuChildThread(params_, gpu_info, gpu_feature_info);

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
  return new InProcessGpuThread(params);
}

}  // namespace content
