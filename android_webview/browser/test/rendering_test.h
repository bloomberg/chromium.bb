// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_
#define ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_

#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/test/fake_window.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}

namespace content {
class TestSynchronousCompositor;
}

namespace android_webview {

class BrowserViewRenderer;
class FakeWindow;
struct ParentCompositorDrawConstraints;

class RenderingTest : public testing::Test,
                      public BrowserViewRendererClient,
                      public WindowHooks {
 public:
  // BrowserViewRendererClient overrides.
  bool RequestDrawGL(bool wait_for_completion) override;
  void OnNewPicture() override;
  void PostInvalidate() override;
  void DetachFunctorFromView() override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(gfx::Vector2d new_value) override {}
  bool IsFlingActive() const override;
  void UpdateScrollState(gfx::Vector2d max_scroll_offset,
                         gfx::SizeF contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override {}
  void DidOverscroll(gfx::Vector2d overscroll_delta) override {}
  void ParentDrawConstraintsUpdated(
      const ParentCompositorDrawConstraints& draw_constraints) override {}
  // WindowHooks overrides.
  void WillOnDraw() override {}
  void DidOnDraw(bool success) override {}
  void WillSyncOnRT(SharedRendererState* functor) override {}
  void DidSyncOnRT(SharedRendererState* functor) override {}
  void WillProcessOnRT(SharedRendererState* functor) override {}
  void DidProcessOnRT(SharedRendererState* functor) override {}
  bool WillDrawOnRT(SharedRendererState* functor,
                    AwDrawGLInfo* draw_info) override;
  void DidDrawOnRT(SharedRendererState* functor) override {}

 protected:
  RenderingTest();
  ~RenderingTest() override;

  virtual void SetUpTestHarness();
  virtual void StartTest();

  void RunTest();
  void InitializeCompositor();
  void Attach();
  void EndTest();

  scoped_refptr<base::MessageLoopProxy> ui_proxy_;
  scoped_ptr<BrowserViewRenderer> browser_view_renderer_;
  scoped_ptr<content::TestSynchronousCompositor> compositor_;
  scoped_ptr<FakeWindow> window_;

 private:
  void QuitMessageLoop();

  const scoped_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RenderingTest);
};

#define RENDERING_TEST_F(TEST_FIXTURE_NAME)         \
  TEST_F(TEST_FIXTURE_NAME, RunTest) { RunTest(); } \
  class NeedsSemicolon##TEST_FIXTURE_NAME {}

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_
