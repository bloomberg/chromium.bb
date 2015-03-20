// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

RenderingTest::RenderingTest() : message_loop_(new base::MessageLoop) {
  ui_proxy_ = base::MessageLoopProxy::current();
}

RenderingTest::~RenderingTest() {
  if (window_.get())
    window_->Detach();
}

void RenderingTest::SetUpTestHarness() {
  DCHECK(!browser_view_renderer_.get());
  browser_view_renderer_.reset(
      new BrowserViewRenderer(this, base::MessageLoopProxy::current()));
  InitializeCompositor();
  Attach();
}

void RenderingTest::InitializeCompositor() {
  DCHECK(!compositor_.get());
  DCHECK(browser_view_renderer_.get());
  compositor_.reset(new content::TestSynchronousCompositor);
  compositor_->SetClient(browser_view_renderer_.get());
}

void RenderingTest::Attach() {
  window_.reset(
      new FakeWindow(browser_view_renderer_.get(), this, gfx::Rect(100, 100)));
}

void RenderingTest::RunTest() {
  SetUpTestHarness();

  ui_proxy_->PostTask(
      FROM_HERE, base::Bind(&RenderingTest::StartTest, base::Unretained(this)));
  message_loop_->Run();
}

void RenderingTest::StartTest() {
  EndTest();
}

void RenderingTest::EndTest() {
  ui_proxy_->PostTask(FROM_HERE, base::Bind(&RenderingTest::QuitMessageLoop,
                                            base::Unretained(this)));
}

void RenderingTest::QuitMessageLoop() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_.get());
  message_loop_->QuitWhenIdle();
}

bool RenderingTest::RequestDrawGL(bool wait_for_completion) {
  window_->RequestDrawGL(wait_for_completion);
  return true;
}

bool RenderingTest::WillDrawOnRT(SharedRendererState* functor,
                                 AwDrawGLInfo* draw_info) {
  draw_info->width = window_->surface_size().width();
  draw_info->height = window_->surface_size().height();
  draw_info->is_layer = false;
  gfx::Transform transform;
  transform.matrix().asColMajorf(draw_info->transform);
  return true;
}

void RenderingTest::OnNewPicture() {
}

void RenderingTest::PostInvalidate() {
  window_->PostInvalidate();
}

void RenderingTest::DetachFunctorFromView() {
}

gfx::Point RenderingTest::GetLocationOnScreen() {
  return gfx::Point();
}

bool RenderingTest::IsFlingActive() const {
  return false;
}

}  // namespace android_webview
