// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

// TODO(nona): Add more tests (crosbug.com/26334).

TEST(IBusUiControllerTest, TestIsActive) {
  InputMethodWhitelist w;
  InputMethodDescriptors descriptors;
  EXPECT_FALSE(IsActiveForTesting("mozc", &descriptors));
  descriptors.push_back(
      InputMethodDescriptor(w, "mozc", "name", "us", "en-US"));
  EXPECT_TRUE(IsActiveForTesting("mozc", &descriptors));
  EXPECT_FALSE(IsActiveForTesting("mozc-jp", &descriptors));
  descriptors.push_back(
      InputMethodDescriptor(w, "xkb:us::eng", "name", "us", "en-US"));
  EXPECT_TRUE(IsActiveForTesting("xkb:us::eng", &descriptors));
  EXPECT_TRUE(IsActiveForTesting("mozc", &descriptors));
  EXPECT_FALSE(IsActiveForTesting("mozc-jp", &descriptors));
}

}  // namespace input_method
}  // namespace chromeos
