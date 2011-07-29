// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/base/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {
namespace {

class ModelTypeTest : public testing::Test {};

TEST_F(ModelTypeTest, ModelTypeToValue) {
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    test::ExpectStringValue(ModelTypeToString(model_type),
                            ModelTypeToValue(model_type));
  }
  test::ExpectStringValue("Top-level folder",
                          ModelTypeToValue(TOP_LEVEL_FOLDER));
  test::ExpectStringValue("Unspecified",
                          ModelTypeToValue(UNSPECIFIED));
}

TEST_F(ModelTypeTest, ModelTypeBitSetToValue) {
  ModelTypeBitSet model_types;
  model_types.set(syncable::BOOKMARKS);
  model_types.set(syncable::APPS);

  scoped_ptr<ListValue> value(ModelTypeBitSetToValue(model_types));
  EXPECT_EQ(2u, value->GetSize());
  std::string types[2];
  EXPECT_TRUE(value->GetString(0, &types[0]));
  EXPECT_TRUE(value->GetString(1, &types[1]));
  EXPECT_EQ("Bookmarks", types[0]);
  EXPECT_EQ("Apps", types[1]);
}

TEST_F(ModelTypeTest, ModelTypeSetToValue) {
  ModelTypeSet model_types;
  model_types.insert(syncable::BOOKMARKS);
  model_types.insert(syncable::APPS);

  scoped_ptr<ListValue> value(ModelTypeSetToValue(model_types));
  EXPECT_EQ(2u, value->GetSize());
  std::string types[2];
  EXPECT_TRUE(value->GetString(0, &types[0]));
  EXPECT_TRUE(value->GetString(1, &types[1]));
  EXPECT_EQ("Bookmarks", types[0]);
  EXPECT_EQ("Apps", types[1]);
}

TEST_F(ModelTypeTest, ModelTypeBitSetFromString) {
  ModelTypeBitSet input, output;
  input.set(BOOKMARKS);
  input.set(AUTOFILL);
  input.set(APPS);
  std::string input_string = input.to_string();
  EXPECT_TRUE(ModelTypeBitSetFromString(input_string, &output));
  EXPECT_EQ(input, output);

  input_string.clear();
  EXPECT_FALSE(ModelTypeBitSetFromString(input_string, &output));

  input_string = "hello world";
  EXPECT_FALSE(ModelTypeBitSetFromString(input_string, &output));

  input_string.clear();
  for (int i = 0; i < MODEL_TYPE_COUNT; ++i)
    input_string += '0' + (i%10);
  EXPECT_FALSE(ModelTypeBitSetFromString(input_string, &output));
}

TEST_F(ModelTypeTest, IsRealDataType) {
  EXPECT_FALSE(IsRealDataType(UNSPECIFIED));
  EXPECT_FALSE(IsRealDataType(MODEL_TYPE_COUNT));
  EXPECT_FALSE(IsRealDataType(TOP_LEVEL_FOLDER));
  EXPECT_TRUE(IsRealDataType(FIRST_REAL_MODEL_TYPE));
  EXPECT_TRUE(IsRealDataType(BOOKMARKS));
  EXPECT_TRUE(IsRealDataType(APPS));
}

}  // namespace
}  // namespace syncable
