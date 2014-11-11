// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

RenderingTest::RenderingTest() : message_loop_(new base::MessageLoop) {
}

RenderingTest::~RenderingTest() {
}

void RenderingTest::SetUpTestHarness() {
  DCHECK(!browser_view_renderer_.get());
  browser_view_renderer_.reset(
      new BrowserViewRenderer(this, base::MessageLoopProxy::current()));
}

class RenderingTest::ScopedInitializeCompositor {
 public:
  explicit ScopedInitializeCompositor(RenderingTest* test) : test_(test) {
    test_->InitializeCompositor();
  }

  ~ScopedInitializeCompositor() { test_->TeardownCompositor(); }

 private:
  RenderingTest* test_;
  DISALLOW_COPY_AND_ASSIGN(ScopedInitializeCompositor);
};

void RenderingTest::InitializeCompositor() {
  DCHECK(!compositor_.get());
  DCHECK(browser_view_renderer_.get());
  compositor_.reset(new content::TestSynchronousCompositor);
  compositor_->SetClient(browser_view_renderer_.get());
}

void RenderingTest::TeardownCompositor() {
  DCHECK(compositor_.get());
  DCHECK(browser_view_renderer_.get());
  compositor_.reset();
}

void RenderingTest::RunTest() {
  ScopedInitializeCompositor initialize_compositor(this);
  StartTest();
}

bool RenderingTest::RequestDrawGL(bool wait_for_completion) {
  return false;
}

void RenderingTest::OnNewPicture() {
}

void RenderingTest::PostInvalidate() {
}

void RenderingTest::InvalidateOnFunctorDestroy() {
}

gfx::Point RenderingTest::GetLocationOnScreen() {
  return gfx::Point();
}

bool RenderingTest::IsFlingActive() const {
  return false;
}

}  // namespace android_webview
