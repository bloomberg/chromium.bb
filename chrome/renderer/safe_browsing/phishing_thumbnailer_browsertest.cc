// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/renderer/safe_browsing/phishing_thumbnailer.h"
#include "content/public/test/render_widget_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {

class ThumbnailerTest : public content::RenderWidgetTest {
 public:
  ThumbnailerTest() {}

 protected:
  virtual void ResizeAndPaint(const gfx::Size& page_size,
                              const gfx::Size& desired_size,
                              SkBitmap* snapshot) OVERRIDE {
    ASSERT_TRUE(snapshot);
    *snapshot = GrabPhishingThumbnail(view_, page_size, desired_size);
    EXPECT_FALSE(snapshot->isNull());
  }
};

TEST_F(ThumbnailerTest, GrabPhishingThumbnail) {
  TestResizeAndPaint();
}

}  // namespace safe_browsing
