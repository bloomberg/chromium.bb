// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/metrics/display_utils.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Crashes on Linux Aura, probably because we need to initialize a Screen
// object. See http://crbug.com/100341
#if defined(USE_AURA) && !defined(OS_WIN)
#define MAYBE_GetPrimaryDisplayDimensions DISABLED_GetPrimaryDisplayDimensions
#else
#define MAYBE_GetPrimaryDisplayDimensions GetPrimaryDisplayDimensions
#endif
TEST(DisplayUtilsTest, MAYBE_GetPrimaryDisplayDimensions) {
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);
  int width = 0, height = 0;
  // We aren't actually testing that it's correct, just that it's sane.
  DisplayUtils::GetPrimaryDisplayDimensions(&width, &height);
  EXPECT_GE(width, 10);
  EXPECT_GE(height, 10);
}

TEST(DisplayUtilsTest, GetDisplayCount) {
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(DisplayUtils::GetDisplayCount(), 1);
}
