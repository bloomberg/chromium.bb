// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/test/rendering_test.h"

namespace android_webview {

class SmokeTest : public RenderingTest {
  void StartTest() override {
    browser_view_renderer_->SetContinuousInvalidate(true);
  }

  void WillOnDraw() override {
    browser_view_renderer_->SetContinuousInvalidate(false);
  }

  void DidDrawOnRT(SharedRendererState* functor) override {
    EndTest();
  }
};

RENDERING_TEST_F(SmokeTest);

class ClearViewTest : public RenderingTest {
 public:
  ClearViewTest() : on_draw_count_(0u) {}

  void StartTest() override {
    browser_view_renderer_->SetContinuousInvalidate(true);
    browser_view_renderer_->ClearView();
  }

  void WillOnDraw() override {
    on_draw_count_++;
    if (on_draw_count_ == 2u) {
      browser_view_renderer_->SetContinuousInvalidate(false);
    }
  }

  void DidOnDraw(bool success) override {
    if (on_draw_count_ == 1u) {
      // First OnDraw should be skipped due to ClearView.
      EXPECT_FALSE(success);
      browser_view_renderer_->DidUpdateContent();  // Unset ClearView.
    } else {
      // Following OnDraws should succeed.
      EXPECT_TRUE(success);
    }
  }

  void DidDrawOnRT(SharedRendererState* functor) override {
    EndTest();
  }
 private:
  size_t on_draw_count_;
};

RENDERING_TEST_F(ClearViewTest);

}  // namespace android_webview
