// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread.h"

#include "content/public/test/test_browser_thread.h"
#include "ios/web/web_thread_impl.h"

namespace web {

// TestWebThreadImpl delegates to content::TestBrowserThread until WebThread
// implementation is indenpendent of content::BrowserThread.
class TestWebThreadImpl {
 public:
  TestWebThreadImpl(WebThread::ID identifier)
      : test_browser_thread_(BrowserThreadIDFromWebThreadID(identifier)) {}

  TestWebThreadImpl(WebThread::ID identifier, base::MessageLoop* message_loop)
      : test_browser_thread_(BrowserThreadIDFromWebThreadID(identifier),
                             message_loop) {}

  ~TestWebThreadImpl() { Stop(); }

  bool Start() { return test_browser_thread_.Start(); }

  bool StartIOThread() { return test_browser_thread_.StartIOThread(); }

  void Stop() { test_browser_thread_.Stop(); }

  bool IsRunning() { return test_browser_thread_.IsRunning(); }

 private:
  content::TestBrowserThread test_browser_thread_;
};

TestWebThread::TestWebThread(WebThread::ID identifier)
    : impl_(new TestWebThreadImpl(identifier)) {
}

TestWebThread::TestWebThread(WebThread::ID identifier,
                             base::MessageLoop* message_loop)
    : impl_(new TestWebThreadImpl(identifier, message_loop)) {
}

TestWebThread::~TestWebThread() {
}

bool TestWebThread::Start() {
  return impl_->Start();
}

bool TestWebThread::StartIOThread() {
  return impl_->StartIOThread();
}

void TestWebThread::Stop() {
  impl_->Stop();
}

bool TestWebThread::IsRunning() {
  return impl_->IsRunning();
}

}  // namespace web
