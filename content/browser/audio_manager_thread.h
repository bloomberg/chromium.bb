// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AUDIO_MANAGER_THREAD_H_
#define CONTENT_BROWSER_AUDIO_MANAGER_THREAD_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {

// This class encapulates the logic for the thread and task runners that the
// AudioManager and related classes run on. audio_task_runner and
// worker_task_runner should be called on the same thread as the
// AudioManagerThread was constructed on.
class CONTENT_EXPORT AudioManagerThread {
 public:
  // Constructs and starts a thread. On all platforms except for Mac, the
  // started thread becomes the audio task runner. However, on Mac, the task
  // runner this object is constructed on will become the audio thread and the
  // thread is only used for high complexity tasks which can't be run on the
  // main thread (typically the UI thread).
  AudioManagerThread();
  ~AudioManagerThread();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  };

  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner() const {
    return thread_.task_runner();
  }

 private:
  base::Thread thread_;

  // Task runner to run audio control calls on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerThread);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AUDIO_MANAGER_THREAD_H_
