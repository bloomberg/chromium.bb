// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"
#include "chromeos/ime/input_method_property.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

bool FindAndUpdateProperty(const InputMethodProperty& new_prop,
                           InputMethodPropertyList* prop_list) {
  return IBusControllerImpl::FindAndUpdatePropertyForTesting(new_prop,
                                                             prop_list);
}

}  // namespace

TEST(IBusControllerImplTest, TestFindAndUpdateProperty) {
  InputMethodPropertyList properties;
  EXPECT_FALSE(FindAndUpdateProperty(InputMethodProperty(), &properties));

  properties.push_back(
      InputMethodProperty("key1", "label1", false, false));
  EXPECT_FALSE(FindAndUpdateProperty(InputMethodProperty(), &properties));
  EXPECT_FALSE(FindAndUpdateProperty(
      InputMethodProperty("keyX", "labelX", false, false), &properties));
  EXPECT_EQ(InputMethodProperty("key1", "label1", false, false),
            properties[0]);
  EXPECT_TRUE(FindAndUpdateProperty(
      InputMethodProperty("key1", "labelY", false, false), &properties));
  EXPECT_EQ(InputMethodProperty("key1", "labelY", false, false),
            properties[0]);

  properties.push_back(
      InputMethodProperty("key2", "label2", false, false));
  EXPECT_FALSE(FindAndUpdateProperty(InputMethodProperty(), &properties));
  EXPECT_FALSE(FindAndUpdateProperty(
      InputMethodProperty("keyX", "labelX", false, false), &properties));
  EXPECT_EQ(InputMethodProperty("key2", "label2", false, false),
            properties[1]);
  EXPECT_TRUE(FindAndUpdateProperty(
      InputMethodProperty("key2", "labelZ", false, false), &properties));
  EXPECT_EQ(InputMethodProperty("key2", "labelZ", false, false),
            properties[1]);
}

}  // namespace input_method
}  // namespace chromeos
