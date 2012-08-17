// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webview_animating_overlay.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/webkit_glue.h"

typedef ChromeRenderViewTest WebViewAnimatingOverlayTest;

TEST_F(WebViewAnimatingOverlayTest, ShowHide) {
  WebViewAnimatingOverlay overlay(view_);
  EXPECT_EQ(overlay.state(), WebViewAnimatingOverlay::HIDDEN);
  overlay.Show();
  EXPECT_EQ(overlay.state(), WebViewAnimatingOverlay::ANIMATING_IN);
  overlay.Hide();
  EXPECT_EQ(overlay.state(), WebViewAnimatingOverlay::ANIMATING_OUT);
}

TEST_F(WebViewAnimatingOverlayTest, Paint) {
  gfx::Size view_size(100, 100);
  Resize(view_size, gfx::Rect(), false);

  WebViewAnimatingOverlay overlay(view_);
  overlay.Show();

  skia::PlatformCanvas canvas;
  ASSERT_TRUE(canvas.initialize(view_size.width(), view_size.height(), true));
  overlay.paintPageOverlay(webkit_glue::ToWebCanvas(&canvas));
}
