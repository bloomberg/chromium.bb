// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_provider_initializer.h"

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ios {

TestChromeProviderInitializer::TestChromeProviderInitializer() {
  chrome_browser_provider_.reset(new TestChromeBrowserProvider());
  ios::SetChromeBrowserProvider(chrome_browser_provider_.get());
}

TestChromeProviderInitializer::~TestChromeProviderInitializer() {
  EXPECT_EQ(chrome_browser_provider_.get(), ios::GetChromeBrowserProvider());
  ios::SetChromeBrowserProvider(nullptr);
}

}  // namespace ios
