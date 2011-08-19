// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_safe_worker.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class ModelSafeWorkerTest : public ::testing::Test {
};

TEST_F(ModelSafeWorkerTest, ModelSafeRoutingInfoToValue) {
  ModelSafeRoutingInfo routing_info;
  routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
  routing_info[syncable::NIGORI] = GROUP_UI;
  routing_info[syncable::PREFERENCES] = GROUP_DB;
  DictionaryValue expected_value;
  expected_value.SetString("Bookmarks", "GROUP_PASSIVE");
  expected_value.SetString("Encryption keys", "GROUP_UI");
  expected_value.SetString("Preferences", "GROUP_DB");
  scoped_ptr<DictionaryValue> value(
      ModelSafeRoutingInfoToValue(routing_info));
  EXPECT_TRUE(value->Equals(&expected_value));
}

TEST_F(ModelSafeWorkerTest, ModelSafeRoutingInfoToString) {
  ModelSafeRoutingInfo routing_info;
  routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
  routing_info[syncable::NIGORI] = GROUP_UI;
  routing_info[syncable::PREFERENCES] = GROUP_DB;
  EXPECT_EQ(
      "{\"Bookmarks\":\"GROUP_PASSIVE\",\"Encryption keys\":\"GROUP_UI\","
      "\"Preferences\":\"GROUP_DB\"}",
      ModelSafeRoutingInfoToString(routing_info));
}

TEST_F(ModelSafeWorkerTest, GetRoutingInfoTypes) {
  ModelSafeRoutingInfo routing_info;
  routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
  routing_info[syncable::NIGORI] = GROUP_UI;
  routing_info[syncable::PREFERENCES] = GROUP_DB;
  syncable::ModelTypeSet expected_types;
  expected_types.insert(syncable::BOOKMARKS);
  expected_types.insert(syncable::NIGORI);
  expected_types.insert(syncable::PREFERENCES);
  EXPECT_EQ(expected_types, GetRoutingInfoTypes(routing_info));
}

}  // namespace
}  // namespace browser_sync
