// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

// This class hosts CastSender and connects it to audio/video frame input
// and network socket.
// This class is created on the main thread and destroyed on the IO
// thread. All methods are accessible only on the IO thread.
class CastSessionDelegate {
 public:
  CastSessionDelegate();
  ~CastSessionDelegate();

 private:
  // Proxy to the IO message loop.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
