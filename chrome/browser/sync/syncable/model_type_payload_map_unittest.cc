// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type_payload_map.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {
namespace {

using test::ExpectBooleanValue;
using test::ExpectDictionaryValue;
using test::ExpectIntegerValue;
using test::ExpectListValue;
using test::ExpectStringValue;

class ModelTypePayloadMapTest : public testing::Test {};

TEST_F(ModelTypePayloadMapTest, TypePayloadMapToValue) {
  ModelTypePayloadMap payloads;
  payloads[syncable::BOOKMARKS] = "bookmarkpayload";
  payloads[syncable::APPS] = "";

  scoped_ptr<DictionaryValue> value(ModelTypePayloadMapToValue(payloads));
  EXPECT_EQ(2u, value->size());
  ExpectStringValue("bookmarkpayload", *value, "Bookmarks");
  ExpectStringValue("", *value, "Apps");
  EXPECT_FALSE(value->HasKey("Preferences"));
}

}  // namespace
}  // namespace syncable
