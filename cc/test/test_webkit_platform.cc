// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_webkit_platform.h"

namespace cc {

TestWebKitPlatform::TestWebKitPlatform() {
}

TestWebKitPlatform::~TestWebKitPlatform() {
}

WebKit::WebCompositorSupport* TestWebKitPlatform::compositorSupport() {
  return &compositor_support_;
}

void TestWebKitPlatform::cryptographicallyRandomValues(
      unsigned char* buffer, size_t length) {
  base::RandBytes(buffer, length);
}

WebKit::WebThread* TestWebKitPlatform::createThread(const char* name) {
  return new webkit_glue::WebThreadImpl(name);
}

WebKit::WebThread* TestWebKitPlatform::currentThread() {
  webkit_glue::WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop)
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop);
  current_thread_slot_.Set(thread);
  return thread;
}

double TestWebKitPlatform::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double TestWebKitPlatform::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

}
