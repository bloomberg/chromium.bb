// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cma_message_loop.h"

#include "base/threading/thread.h"

namespace chromecast {
namespace media {

// static
scoped_refptr<base::MessageLoopProxy> CmaMessageLoop::GetMessageLoopProxy() {
  return GetInstance()->thread_->message_loop_proxy();
}

// static
CmaMessageLoop* CmaMessageLoop::GetInstance() {
  return Singleton<CmaMessageLoop>::get();
}

CmaMessageLoop::CmaMessageLoop()
  : thread_(new base::Thread("CmaThread")) {
  thread_->Start();
}

CmaMessageLoop::~CmaMessageLoop() {
  // This will automatically shutdown the thread.
}

}  // namespace media
}  // namespace chromecast
