// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/threading/thread_local_storage.h"
#include "base/test/test_suite.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/glue/webthread_impl.h"
#include <gmock/gmock.h>

#include "base/debug/trace_event_impl.h"

using webkit_glue::WebThreadImplForMessageLoop;

class TestPlatform : public WebKit::Platform {
 public:
  virtual WebKit::WebCompositorSupport* compositorSupport() {
    return &compositor_support_;
  }

  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length) {
    base::RandBytes(buffer, length);
  }

  virtual WebKit::WebThread* createThread(const char* name) {
    return new webkit_glue::WebThreadImpl(name);
  }

  virtual WebKit::WebThread* currentThread() {
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

  virtual double currentTime() {
    return base::Time::Now().ToDoubleT();
  }

  virtual double monotonicallyIncreasingTime() {
    return base::TimeTicks::Now().ToInternalValue() /
        static_cast<double>(base::Time::kMicrosecondsPerSecond);
  }

 private:
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webkit::WebCompositorSupportImpl compositor_support_;
};

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite testSuite(argc, argv);
  TestPlatform platform;
  MessageLoop message_loop;
  WebKit::Platform::initialize(&platform);
  int result = testSuite.Run();
  WebKit::Platform::shutdown();

  return result;
}

