// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"
#include "content/public/test/content_browser_test.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

typedef ContentBrowserTest CompositorUtilTest;

// Test that compositing is in the expected mode on the bots for all platforms.
IN_PROC_BROWSER_TEST_F(CompositorUtilTest, CompositingModeAsExpected) {
  enum CompositingMode {
    DIRECT,
    DELEGATED,
  } expected_mode = DIRECT;
#if defined(USE_AURA) || defined(OS_ANDROID)
  expected_mode = DELEGATED;
#elif defined(OS_MACOSX)
  expected_mode = DELEGATED;
#endif

  EXPECT_EQ(expected_mode == DELEGATED, IsDelegatedRendererEnabled());
}

}
