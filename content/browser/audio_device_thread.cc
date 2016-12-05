// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/audio_device_thread.h"

namespace content {

scoped_refptr<base::SingleThreadTaskRunner> AudioDeviceThread::GetTaskRunner() {
#if defined(OS_MACOSX)
    // On Mac audio task runner must belong to the main thread.
    // See http://crbug.com/158170.
    return base::ThreadTaskRunnerHandle::Get();
#else
    return thread_.task_runner();
#endif  // defined(OS_MACOSX)
  }

AudioDeviceThread::AudioDeviceThread() : thread_("AudioThread") {
#if defined(OS_WIN)
  thread_.init_com_with_mta(true);
#endif
  CHECK(thread_.Start());
}

}  // namespace content
