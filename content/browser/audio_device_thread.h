// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AUDIO_DEVICE_THREAD_H_
#define CONTENT_BROWSER_AUDIO_DEVICE_THREAD_H_

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {

// This class encapulates the logic for the thread and task runners that the
// AudioManager and related classes run on. audio_task_runner and
// worker_task_runner should be called on the same thread as the
// AudioDeviceThread was constructed on.
class CONTENT_EXPORT AudioDeviceThread {
 public:
  // Constructs and starts the thread.
  AudioDeviceThread();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner() {
    return thread_.task_runner();
  }

 private:
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceThread);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AUDIO_DEVICE_THREAD_H_
