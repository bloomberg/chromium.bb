// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable_id.h"

#include <vector>

#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;

namespace syncable {

using browser_sync::TestIdFactory;

class SyncableIdTest : public testing::Test { };

TEST(SyncableIdTest, TestIDCreation) {
  vector<Id> v;
  v.push_back(TestIdFactory::FromNumber(5));
  v.push_back(TestIdFactory::FromNumber(1));
  v.push_back(TestIdFactory::FromNumber(-5));
  v.push_back(TestIdFactory::MakeLocal("A"));
  v.push_back(TestIdFactory::MakeLocal("B"));
  v.push_back(TestIdFactory::MakeServer("A"));
  v.push_back(TestIdFactory::MakeServer("B"));
  v.push_back(Id::CreateFromServerId("-5"));
  v.push_back(Id::CreateFromClientString("A"));
  v.push_back(Id::CreateFromServerId("A"));

  for (vector<Id>::iterator i = v.begin(); i != v.end(); ++i) {
    for (vector<Id>::iterator j = v.begin(); j != i; ++j) {
      ASSERT_NE(*i, *j) << "mis equated two distinct ids";
    }
    ASSERT_EQ(*i, *i) << "self-equality failed";
    Id copy1 = *i;
    Id copy2 = *i;
    ASSERT_EQ(copy1, copy2) << "equality after copy failed";
  }
}

}  // namespace syncable
