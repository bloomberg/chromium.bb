// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_test_support.h"

namespace chromeos {
namespace input_method {

class InputMethodConfigurationTest : public testing::Test {
 public:
  void SetUp() {
  }

  void TearDown() {
  }
};

TEST_F(InputMethodConfigurationTest, TestInitialize) {
  Initialize(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  InputMethodManager* manager = GetInputMethodManager();
  EXPECT_TRUE(manager);
  Shutdown();
}

TEST_F(InputMethodConfigurationTest, TestInitializeForTesting) {
  InitializeForTesting(new MockInputMethodManager);
  InputMethodManager* manager = GetInputMethodManager();
  EXPECT_TRUE(manager);
  Shutdown();
}

}  // namespace input_method
}  // namespace chromeos
