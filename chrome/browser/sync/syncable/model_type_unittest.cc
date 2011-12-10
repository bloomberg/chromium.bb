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

TEST_F(ModelTypeTest, ModelEnumSetToValue) {
  const ModelEnumSet model_types(syncable::BOOKMARKS, syncable::APPS);

  scoped_ptr<ListValue> value(ModelEnumSetToValue(model_types));
  EXPECT_EQ(2u, value->GetSize());
  std::string types[2];
  EXPECT_TRUE(value->GetString(0, &types[0]));
  EXPECT_TRUE(value->GetString(1, &types[1]));
  EXPECT_EQ("Bookmarks", types[0]);
  EXPECT_EQ("Apps", types[1]);
}

TEST_F(ModelTypeTest, ModelEnumSetFromValue) {
  // Try empty set first.
  ModelEnumSet model_types;
  scoped_ptr<ListValue> value(ModelEnumSetToValue(model_types));
  EXPECT_TRUE(model_types.Equals(ModelEnumSetFromValue(*value)));

  // Now try with a few random types.
  model_types.Put(BOOKMARKS);
  model_types.Put(APPS);
  value.reset(ModelEnumSetToValue(model_types));
  EXPECT_TRUE(model_types.Equals(ModelEnumSetFromValue(*value)));
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
