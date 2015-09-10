// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_message_loop.h"

#include "base/threading/thread.h"

namespace chromecast {
namespace media {

// static
scoped_refptr<base::SingleThreadTaskRunner> MediaMessageLoop::GetTaskRunner() {
  return GetInstance()->thread_->task_runner();
}

// static
MediaMessageLoop* MediaMessageLoop::GetInstance() {
  return base::Singleton<MediaMessageLoop>::get();
}

MediaMessageLoop::MediaMessageLoop()
  : thread_(new base::Thread("CmaThread")) {
  thread_->Start();
}

MediaMessageLoop::~MediaMessageLoop() {
  // This will automatically shutdown the thread.
}

}  // namespace media
}  // namespace chromecast
