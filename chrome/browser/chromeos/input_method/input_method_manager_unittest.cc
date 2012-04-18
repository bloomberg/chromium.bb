// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(InputMethodManagerTest, TestInitialize) {
  InputMethodManager::Initialize();
  InputMethodManager* manager = InputMethodManager::GetInstance();
  EXPECT_TRUE(manager);
  InputMethodManager::Shutdown();
}

TEST(InputMethodManagerTest, TestInitializeForTesting) {
  InputMethodManager::InitializeForTesting(new MockInputMethodManager);
  InputMethodManager* manager = InputMethodManager::GetInstance();
  EXPECT_TRUE(manager);
  InputMethodManager::Shutdown();
}

}  // namespace input_method
}  // namespace chromeos
