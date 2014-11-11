// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_
#define ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_

#include "android_webview/browser/browser_view_renderer_client.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}

namespace content {
class TestSynchronousCompositor;
}

namespace android_webview {

class BrowserViewRenderer;

class RenderingTest : public testing::Test, public BrowserViewRendererClient {
 public:
  // BrowserViewRendererClient overrides.
  bool RequestDrawGL(bool wait_for_completion) override;
  void OnNewPicture() override;
  void PostInvalidate() override;
  void InvalidateOnFunctorDestroy() override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(gfx::Vector2d new_value) override {}
  bool IsFlingActive() const override;
  void UpdateScrollState(gfx::Vector2d max_scroll_offset,
                         gfx::SizeF contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override {}
  void DidOverscroll(gfx::Vector2d overscroll_delta) override {}

 protected:
  RenderingTest();
  ~RenderingTest() override;

  void SetUpTestHarness();
  void RunTest();

  virtual void StartTest() {}

 private:
  class ScopedInitializeCompositor;
  void InitializeCompositor();
  void TeardownCompositor();

  const scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<content::TestSynchronousCompositor> compositor_;
  scoped_ptr<BrowserViewRenderer> browser_view_renderer_;

  DISALLOW_COPY_AND_ASSIGN(RenderingTest);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TEST_RENDERING_TEST_H_
