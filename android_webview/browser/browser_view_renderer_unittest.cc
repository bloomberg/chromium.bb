// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/rendering_test.h"

namespace android_webview {

TEST_F(RenderingTest, SmokeTest) {
  SetUpTestHarness();
  RunTest();
}

}  // namespace android_webview
