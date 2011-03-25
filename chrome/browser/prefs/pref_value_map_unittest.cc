// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefValueMapTest : public testing::Test {
};

TEST_F(PrefValueMapTest, SetValue) {
  PrefValueMap map;
  const Value* result = NULL;
  EXPECT_FALSE(map.GetValue("key", &result));
  EXPECT_FALSE(result);

  EXPECT_TRUE(map.SetValue("key", Value::CreateStringValue("test")));
  EXPECT_FALSE(map.SetValue("key", Value::CreateStringValue("test")));
  EXPECT_TRUE(map.SetValue("key", Value::CreateStringValue("hi mom!")));

  EXPECT_TRUE(map.GetValue("key", &result));
  EXPECT_TRUE(StringValue("hi mom!").Equals(result));
}

TEST_F(PrefValueMapTest, RemoveValue) {
  PrefValueMap map;
  EXPECT_FALSE(map.RemoveValue("key"));

  EXPECT_TRUE(map.SetValue("key", Value::CreateStringValue("test")));
  EXPECT_TRUE(map.GetValue("key", NULL));

  EXPECT_TRUE(map.RemoveValue("key"));
  EXPECT_FALSE(map.GetValue("key", NULL));

  EXPECT_FALSE(map.RemoveValue("key"));
}

TEST_F(PrefValueMapTest, Clear) {
  PrefValueMap map;
  EXPECT_TRUE(map.SetValue("key", Value::CreateStringValue("test")));
  EXPECT_TRUE(map.GetValue("key", NULL));

  map.Clear();

  EXPECT_FALSE(map.GetValue("key", NULL));
}

TEST_F(PrefValueMapTest, GetDifferingKeys) {
  PrefValueMap reference;
  EXPECT_TRUE(reference.SetValue("b", Value::CreateStringValue("test")));
  EXPECT_TRUE(reference.SetValue("c", Value::CreateStringValue("test")));
  EXPECT_TRUE(reference.SetValue("e", Value::CreateStringValue("test")));

  PrefValueMap check;
  std::vector<std::string> differing_paths;
  std::vector<std::string> expected_differing_paths;

  reference.GetDifferingKeys(&check, &differing_paths);
  expected_differing_paths.push_back("b");
  expected_differing_paths.push_back("c");
  expected_differing_paths.push_back("e");
  EXPECT_EQ(expected_differing_paths, differing_paths);

  EXPECT_TRUE(check.SetValue("a", Value::CreateStringValue("test")));
  EXPECT_TRUE(check.SetValue("c", Value::CreateStringValue("test")));
  EXPECT_TRUE(check.SetValue("d", Value::CreateStringValue("test")));

  reference.GetDifferingKeys(&check, &differing_paths);
  expected_differing_paths.clear();
  expected_differing_paths.push_back("a");
  expected_differing_paths.push_back("b");
  expected_differing_paths.push_back("d");
  expected_differing_paths.push_back("e");
  EXPECT_EQ(expected_differing_paths, differing_paths);
}
