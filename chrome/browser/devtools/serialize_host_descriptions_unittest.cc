// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/serialize_host_descriptions.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

HostDescriptionNode GetDummyNode() {
  return {std::string(), std::string(), base::DictionaryValue()};
}

HostDescriptionNode GetNodeWithLabel(int id) {
  base::DictionaryValue dict;
  dict.SetInteger("label", id);
  return {std::string(), std::string(), std::move(dict)};
}

int GetLabel(const base::DictionaryValue* dict) {
  int result = 0;
  EXPECT_TRUE(dict->GetInteger("label", &result));
  return result;
}

}  // namespace

TEST(SerializeDictionaryForestTest, Empty) {
  base::ListValue result =
      SerializeHostDescriptions(std::vector<HostDescriptionNode>(), "123");
  EXPECT_TRUE(result.empty());
}

// Test serializing a forest of stubs (no edges).
TEST(SerializeDictionaryForestTest, Stubs) {
  base::ListValue result = SerializeHostDescriptions(
      {GetDummyNode(), GetDummyNode(), GetDummyNode()}, "123");
  EXPECT_EQ(3u, result.GetSize());
  for (const base::Value& value : result) {
    const base::DictionaryValue* dict = nullptr;
    ASSERT_TRUE(value.GetAsDictionary(&dict));
    EXPECT_FALSE(dict->HasKey("123"));
  }
}

// Test serializing a small forest, of this structure:
// 5 -- 2 -- 4
// 0 -- 6
//   \ 1
//   \ 3
TEST(SerializeDictionaryForestTest, Forest) {
  std::vector<HostDescriptionNode> nodes(7);
  const char* kNames[] = {"0", "1", "2", "3", "4", "5", "6"};
  for (size_t i = 0; i < 7; ++i) {
    nodes[i] = GetNodeWithLabel(i);
    nodes[i].name = kNames[i];
  }
  nodes[2].parent_name = "5";
  nodes[4].parent_name = "2";
  nodes[6].parent_name = "0";
  nodes[1].parent_name = "0";
  nodes[3].parent_name = "0";

  base::ListValue result =
      SerializeHostDescriptions(std::move(nodes), "children");

  EXPECT_EQ(2u, result.GetSize());
  const base::Value* value = nullptr;
  const base::DictionaryValue* dict = nullptr;
  const base::ListValue* list = nullptr;

  // Check the result. Note that sibling nodes are in the same order in which
  // they appear in |nodes|.

  // Node 0
  ASSERT_TRUE(result.Get(0, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  EXPECT_EQ(0, GetLabel(dict));
  ASSERT_TRUE(dict->GetList("children", &list));
  EXPECT_EQ(3u, list->GetSize());

  // Nodes 1, 3, 6
  constexpr int kLabels[] = {1, 3, 6};
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(list->Get(i, &value));
    ASSERT_TRUE(value->GetAsDictionary(&dict));
    EXPECT_EQ(kLabels[i], GetLabel(dict));
    EXPECT_FALSE(dict->HasKey("children"));
  }

  // Node 5
  ASSERT_TRUE(result.Get(1, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  EXPECT_EQ(5, GetLabel(dict));
  ASSERT_TRUE(dict->GetList("children", &list));
  EXPECT_EQ(1u, list->GetSize());

  // Node 2
  ASSERT_TRUE(list->Get(0, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  EXPECT_EQ(2, GetLabel(dict));
  ASSERT_TRUE(dict->GetList("children", &list));
  EXPECT_EQ(1u, list->GetSize());

  // Node 4
  ASSERT_TRUE(list->Get(0, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  EXPECT_EQ(4, GetLabel(dict));
  EXPECT_FALSE(dict->HasKey("children"));
}
