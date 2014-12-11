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

}  // namespace android_webview
