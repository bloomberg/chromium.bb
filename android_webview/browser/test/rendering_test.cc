// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/render_thread_manager.h"
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
  DCHECK(!render_thread_manager_.get());
  render_thread_manager_.reset(
      new RenderThreadManager(this, base::ThreadTaskRunnerHandle::Get()));
  browser_view_renderer_.reset(new BrowserViewRenderer(
      this, base::ThreadTaskRunnerHandle::Get(), false));
  browser_view_renderer_->SetRenderThreadManager(render_thread_manager_.get());
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
  window_.reset(new FakeWindow(browser_view_renderer_.get(),
                               render_thread_manager_.get(), this,
                               gfx::Rect(100, 100)));
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

std::unique_ptr<cc::CompositorFrame> RenderingTest::ConstructEmptyFrame() {
  std::unique_ptr<cc::CompositorFrame> compositor_frame(
      new cc::CompositorFrame);
  std::unique_ptr<cc::DelegatedFrameData> frame(new cc::DelegatedFrameData);
  std::unique_ptr<cc::RenderPass> root_pass(cc::RenderPass::Create());
  gfx::Rect viewport(browser_view_renderer_->size());
  root_pass->SetNew(cc::RenderPassId(1, 1), viewport, viewport,
                    gfx::Transform());
  frame->render_pass_list.push_back(std::move(root_pass));
  compositor_frame->delegated_frame_data = std::move(frame);
  return compositor_frame;
}

void RenderingTest::WillOnDraw() {
  DCHECK(compositor_);
  compositor_->SetHardwareFrame(0u, ConstructEmptyFrame());
}

bool RenderingTest::RequestInvokeGL(bool wait_for_completion) {
  window_->RequestInvokeGL(wait_for_completion);
  return true;
}

bool RenderingTest::WillDrawOnRT(RenderThreadManager* functor,
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
  if (window_)
    window_->PostInvalidate();
}

void RenderingTest::OnParentDrawConstraintsUpdated() {
  browser_view_renderer_->OnParentDrawConstraintsUpdated();
}

void RenderingTest::DetachFunctorFromView() {
}

gfx::Point RenderingTest::GetLocationOnScreen() {
  return gfx::Point();
}

}  // namespace android_webview
