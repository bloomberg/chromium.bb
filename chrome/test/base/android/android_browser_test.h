// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_

#include "content/public/test/browser_test_base.h"

class AndroidBrowserTest : public content::BrowserTestBase {
 public:
  AndroidBrowserTest();
  ~AndroidBrowserTest() override;

  // content::BrowserTestBase implementation.
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;
};

#endif  // CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
