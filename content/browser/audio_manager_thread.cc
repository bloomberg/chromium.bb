// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/audio_manager_thread.h"

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

namespace content {

AudioManagerThread::AudioManagerThread() : thread_("AudioThread") {
#if defined(OS_WIN)
  thread_.init_com_with_mta(true);
#endif
  CHECK(thread_.Start());

#if defined(OS_MACOSX)
  // On Mac, the audio task runner must belong to the main thread.
  // See http://crbug.com/158170.
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
#else
  task_runner_ = thread_.task_runner();
#endif
}

AudioManagerThread::~AudioManagerThread() {}

}  // namespace content
