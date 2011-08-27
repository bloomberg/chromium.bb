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

TEST_F(ModelTypeTest, ModelTypeFromValue) {
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    scoped_ptr<StringValue> value(ModelTypeToValue(model_type));
    EXPECT_EQ(model_type, ModelTypeFromValue(*value));
  }
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

TEST_F(ModelTypeTest, ModelTypeBitSetFromValue) {
  // Try empty set first.
  ModelTypeBitSet model_types;
  scoped_ptr<ListValue> value(ModelTypeBitSetToValue(model_types));
  EXPECT_EQ(model_types, ModelTypeBitSetFromValue(*value));

  // Now try with a few random types.
  model_types.set(syncable::BOOKMARKS);
  model_types.set(syncable::APPS);
  value.reset(ModelTypeBitSetToValue(model_types));
  EXPECT_EQ(model_types, ModelTypeBitSetFromValue(*value));
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

TEST_F(ModelTypeTest, ModelTypeSetFromValue) {
  // Try empty set first.
  ModelTypeSet model_types;
  scoped_ptr<ListValue> value(ModelTypeSetToValue(model_types));
  EXPECT_EQ(model_types, ModelTypeSetFromValue(*value));

  // Now try with a few random types.
  model_types.insert(BOOKMARKS);
  model_types.insert(APPS);
  value.reset(ModelTypeSetToValue(model_types));
  EXPECT_EQ(model_types, ModelTypeSetFromValue(*value));
}

TEST_F(ModelTypeTest, GetAllRealModelTypes) {
  const ModelTypeSet& all_types = GetAllRealModelTypes();
  for (int i = 0; i < MODEL_TYPE_COUNT; ++i) {
    ModelType type = ModelTypeFromInt(i);
    EXPECT_EQ(IsRealDataType(type), all_types.count(type) > 0u);
  }
  EXPECT_EQ(0u, all_types.count(MODEL_TYPE_COUNT));
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
