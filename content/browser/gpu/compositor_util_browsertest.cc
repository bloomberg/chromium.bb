// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"
#include "content/test/content_browser_test.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

typedef ContentBrowserTest CompositorUtilTest;

// Test that threaded compositing and FCM are in the expected mode on the bots
// for all platforms.
IN_PROC_BROWSER_TEST_F(CompositorUtilTest, CompositingModeAsExpected) {
  enum CompositingMode {
    DISABLED,
    ENABLED,
    THREADED,
  } expected_mode = DISABLED;
#if defined(OS_ANDROID) || defined(USE_AURA)
  expected_mode = THREADED;
#elif defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    expected_mode = ENABLED;
#endif

  EXPECT_EQ(expected_mode == ENABLED || expected_mode == THREADED,
            IsForceCompositingModeEnabled());
  EXPECT_EQ(expected_mode == THREADED, IsThreadedCompositingEnabled());
}

}
