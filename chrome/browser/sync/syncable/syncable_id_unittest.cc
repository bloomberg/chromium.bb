// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable_id.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
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

TEST(SyncableIdTest, GetLeastIdForLexicographicComparison) {
  vector<Id> v;
  v.push_back(Id::CreateFromServerId("z5"));
  v.push_back(Id::CreateFromServerId("z55"));
  v.push_back(Id::CreateFromServerId("z6"));
  v.push_back(Id::CreateFromClientString("zA-"));
  v.push_back(Id::CreateFromClientString("zA--"));
  v.push_back(Id::CreateFromServerId("zA--"));

  for (int i = 0; i <= 255; ++i) {
    std::string one_character_id;
    one_character_id.push_back(i);
    v.push_back(Id::CreateFromClientString(one_character_id));
  }

  for (vector<Id>::iterator i = v.begin(); i != v.end(); ++i) {
    // The following looks redundant, but we're testing a custom operator<.
    ASSERT_LT(Id::GetLeastIdForLexicographicComparison(), *i);
    ASSERT_NE(*i, i->GetLexicographicSuccessor());
    ASSERT_NE(i->GetLexicographicSuccessor(), *i);
    ASSERT_LT(*i, i->GetLexicographicSuccessor());
    ASSERT_GT(i->GetLexicographicSuccessor(), *i);
    for (vector<Id>::iterator j = v.begin(); j != v.end(); ++j) {
      if (j == i)
        continue;
      if (*j < *i) {
        ASSERT_LT(j->GetLexicographicSuccessor(), *i);
        ASSERT_LT(j->GetLexicographicSuccessor(),
            i->GetLexicographicSuccessor());
        ASSERT_LT(*j, i->GetLexicographicSuccessor());
      } else {
        ASSERT_GT(j->GetLexicographicSuccessor(), *i);
        ASSERT_GT(j->GetLexicographicSuccessor(),
            i->GetLexicographicSuccessor());
        ASSERT_GT(*j, i->GetLexicographicSuccessor());
      }
    }
  }
}

namespace {

// TODO(akalin): Move this to values_test_util.h.

// Takes ownership of |actual|.
void ExpectStringValue(const std::string& expected_str,
                       StringValue* actual) {
  scoped_ptr<StringValue> scoped_actual(actual);
  std::string actual_str;
  EXPECT_TRUE(scoped_actual->GetAsString(&actual_str));
  EXPECT_EQ(expected_str, actual_str);
}

}  // namespace

TEST(SyncableIdTest, ToValue) {
  ExpectStringValue("r", Id::CreateFromServerId("0").ToValue());
  ExpectStringValue("svalue", Id::CreateFromServerId("value").ToValue());

  ExpectStringValue("r", Id::CreateFromClientString("0").ToValue());
  ExpectStringValue("cvalue", Id::CreateFromClientString("value").ToValue());
}

}  // namespace syncable
