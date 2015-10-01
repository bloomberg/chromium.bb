// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

RenderingTest::RenderingTest() : message_loop_(new base::MessageLoop) {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

RenderingTest::~RenderingTest() {
  if (window_.get())
    window_->Detach();
}

void RenderingTest::SetUpTestHarness() {
  DCHECK(!browser_view_renderer_.get());
  browser_view_renderer_.reset(new BrowserViewRenderer(
      this, base::ThreadTaskRunnerHandle::Get(), false));
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

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RenderingTest::StartTest, base::Unretained(this)));
  message_loop_->Run();
}

void RenderingTest::StartTest() {
  EndTest();
}

void RenderingTest::EndTest() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RenderingTest::QuitMessageLoop, base::Unretained(this)));
}

void RenderingTest::QuitMessageLoop() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_.get());
  message_loop_->QuitWhenIdle();
}

void RenderingTest::SetCompositorFrame() {
  DCHECK(compositor_.get());
  scoped_ptr<cc::CompositorFrame> compositor_frame(new cc::CompositorFrame);
  scoped_ptr<cc::DelegatedFrameData> frame(new cc::DelegatedFrameData);
  scoped_ptr<cc::RenderPass> root_pass(cc::RenderPass::Create());
  gfx::Rect viewport(browser_view_renderer_->size());
  root_pass->SetNew(cc::RenderPassId(1, 1), viewport, viewport,
                    gfx::Transform());
  frame->render_pass_list.push_back(root_pass.Pass());
  compositor_frame->delegated_frame_data = frame.Pass();
  compositor_->SetHardwareFrame(compositor_frame.Pass());
}

void RenderingTest::WillOnDraw() {
  SetCompositorFrame();
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

}  // namespace android_webview
