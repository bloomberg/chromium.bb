// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/common/value_counter.h"
#include "testing/gtest/include/gtest/gtest.h"

class ValueCounterUnittest : public testing::Test {
};

TEST_F(ValueCounterUnittest, TestAddingSameValue) {
  extensions::ValueCounter vc;
  base::ListValue value;
  ASSERT_EQ(1, vc.Add(value));
  ASSERT_EQ(2, vc.Add(value));
}

TEST_F(ValueCounterUnittest, TestAddingDifferentValue) {
  extensions::ValueCounter vc;
  base::ListValue value1;
  base::DictionaryValue value2;
  ASSERT_EQ(1, vc.Add(value1));
  ASSERT_EQ(1, vc.Add(value2));
}

TEST_F(ValueCounterUnittest, TestRemovingValue) {
  extensions::ValueCounter vc;
  base::ListValue value;
  ASSERT_EQ(1, vc.Add(value));
  ASSERT_EQ(2, vc.Add(value));
  ASSERT_EQ(1, vc.Remove(value));
  ASSERT_EQ(0, vc.Remove(value));
}

TEST_F(ValueCounterUnittest, TestAddIfMissing) {
  extensions::ValueCounter vc;
  base::ListValue value;
  ASSERT_EQ(1, vc.AddIfMissing(value));
  ASSERT_EQ(1, vc.AddIfMissing(value));
}
