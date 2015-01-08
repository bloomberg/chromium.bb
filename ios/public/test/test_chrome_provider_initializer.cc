// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/test_chrome_provider_initializer.h"

#include "ios/public/test/test_chrome_browser_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ios {

TestChromeProviderInitializer::TestChromeProviderInitializer() {
  chrome_browser_provider_ = new TestChromeBrowserProvider();
  ios::SetChromeBrowserProvider(chrome_browser_provider_);
}

TestChromeProviderInitializer::~TestChromeProviderInitializer() {
  EXPECT_EQ(chrome_browser_provider_, ios::GetChromeBrowserProvider());
  ios::SetChromeBrowserProvider(NULL);
  delete chrome_browser_provider_;
}

}  // namespace ios
