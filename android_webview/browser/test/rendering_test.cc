// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

namespace {
// BrowserViewRenderer subclass used for enabling tests to observe
// OnParentDrawConstraintsUpdated.
class TestBrowserViewRenderer : public BrowserViewRenderer {
 public:
  TestBrowserViewRenderer(
      RenderingTest* rendering_test,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : BrowserViewRenderer(rendering_test, ui_task_runner),
        rendering_test_(rendering_test) {}

  ~TestBrowserViewRenderer() override {}

  void OnParentDrawConstraintsUpdated(
      CompositorFrameConsumer* compositor_frame_consumer) override {
    BrowserViewRenderer::OnParentDrawConstraintsUpdated(
        compositor_frame_consumer);
    rendering_test_->OnParentDrawConstraintsUpdated();
  }

 private:
  RenderingTest* const rendering_test_;
};
}

RenderingTest::RenderingTest() : message_loop_(new base::MessageLoop) {
  // TODO(boliu): Update unit tests to async code path.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSyncOnDrawHardware);
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

RenderingTest::~RenderingTest() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (window_.get())
    window_->Detach();
}

ui::TouchHandleDrawable* RenderingTest::CreateDrawable() {
  return nullptr;
}

void RenderingTest::SetUpTestHarness() {
  DCHECK(!browser_view_renderer_.get());
  DCHECK(!functor_.get());
  browser_view_renderer_.reset(
      new TestBrowserViewRenderer(this, base::ThreadTaskRunnerHandle::Get()));
  browser_view_renderer_->SetActiveCompositorID(CompositorID(0, 0));
  InitializeCompositor();
  std::unique_ptr<FakeWindow> window(
      new FakeWindow(browser_view_renderer_.get(), this, gfx::Rect(100, 100)));
  functor_.reset(new FakeFunctor);
  functor_->Init(window.get(),
                 base::MakeUnique<RenderThreadManager>(
                     functor_.get(), base::ThreadTaskRunnerHandle::Get()));
  browser_view_renderer_->SetCurrentCompositorFrameConsumer(
      functor_->GetCompositorFrameConsumer());
  window_ = std::move(window);
}

CompositorFrameConsumer* RenderingTest::GetCompositorFrameConsumer() {
  return functor_->GetCompositorFrameConsumer();
}

CompositorFrameProducer* RenderingTest::GetCompositorFrameProducer() {
  return browser_view_renderer_.get();
}

void RenderingTest::InitializeCompositor() {
  DCHECK(!compositor_.get());
  DCHECK(browser_view_renderer_.get());
  compositor_.reset(new content::TestSynchronousCompositor(0, 0));
  compositor_->SetClient(browser_view_renderer_.get());
}

void RenderingTest::RunTest() {
  SetUpTestHarness();

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RenderingTest::StartTest, base::Unretained(this)));
  run_loop_.Run();
}

void RenderingTest::StartTest() {
  EndTest();
}

void RenderingTest::EndTest() {
  ui_task_runner_->PostTask(FROM_HERE, run_loop_.QuitWhenIdleClosure());
}

content::SynchronousCompositor* RenderingTest::ActiveCompositor() const {
  return browser_view_renderer_->GetActiveCompositorForTesting();
}

std::unique_ptr<cc::CompositorFrame> RenderingTest::ConstructEmptyFrame() {
  std::unique_ptr<cc::CompositorFrame> compositor_frame(
      new cc::CompositorFrame);
  std::unique_ptr<cc::RenderPass> root_pass(cc::RenderPass::Create());
  gfx::Rect viewport(browser_view_renderer_->size());
  root_pass->SetNew(1, viewport, viewport, gfx::Transform());
  compositor_frame->render_pass_list.push_back(std::move(root_pass));
  return compositor_frame;
}

std::unique_ptr<cc::CompositorFrame> RenderingTest::ConstructFrame(
    cc::ResourceId resource_id) {
  std::unique_ptr<cc::CompositorFrame> compositor_frame(ConstructEmptyFrame());
  cc::TransferableResource resource;
  resource.id = resource_id;
  compositor_frame->resource_list.push_back(resource);
  return compositor_frame;
}

FakeFunctor* RenderingTest::GetFunctor() {
  return functor_.get();
}

void RenderingTest::WillOnDraw() {
  DCHECK(compositor_);
  compositor_->SetHardwareFrame(0u, ConstructEmptyFrame());
}

bool RenderingTest::WillDrawOnRT(AwDrawGLInfo* draw_info) {
  draw_info->width = window_->surface_size().width();
  draw_info->height = window_->surface_size().height();
  draw_info->is_layer = false;
  gfx::Transform transform;
  transform.matrix().asColMajorf(draw_info->transform);
  return true;
}

void RenderingTest::OnNewPicture() {}

void RenderingTest::PostInvalidate() {
  if (window_)
    window_->PostInvalidate();
}

gfx::Point RenderingTest::GetLocationOnScreen() {
  return gfx::Point();
}

}  // namespace android_webview
