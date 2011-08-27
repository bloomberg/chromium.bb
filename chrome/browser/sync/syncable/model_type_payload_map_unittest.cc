// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type_payload_map.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/base/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {
namespace {

using test::ExpectDictStringValue;

class ModelTypePayloadMapTest : public testing::Test {};

TEST_F(ModelTypePayloadMapTest, TypePayloadMapToSet) {
  ModelTypePayloadMap payloads;
  payloads[BOOKMARKS] = "bookmarkpayload";
  payloads[APPS] = "";

  ModelTypeSet types;
  types.insert(BOOKMARKS);
  types.insert(APPS);
  EXPECT_EQ(types, ModelTypePayloadMapToSet(payloads));
}

TEST_F(ModelTypePayloadMapTest, TypePayloadMapToValue) {
  ModelTypePayloadMap payloads;
  payloads[BOOKMARKS] = "bookmarkpayload";
  payloads[APPS] = "";

  scoped_ptr<DictionaryValue> value(ModelTypePayloadMapToValue(payloads));
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("bookmarkpayload", *value, "Bookmarks");
  ExpectDictStringValue("", *value, "Apps");
  EXPECT_FALSE(value->HasKey("Preferences"));
}

}  // namespace
}  // namespace syncable
