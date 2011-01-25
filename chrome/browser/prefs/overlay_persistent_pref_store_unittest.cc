// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/prefs/overlay_persistent_pref_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/pref_store_observer_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::StrEq;

const char key[] = "test.key";

class OverlayPersistentPrefStoreTest : public testing::Test {
 public:
  OverlayPersistentPrefStoreTest()
      : underlay_(new TestingPrefStore()),
        overlay_(new OverlayPersistentPrefStore(underlay_.get())) {
  }

  scoped_refptr<TestingPrefStore> underlay_;
  scoped_refptr<OverlayPersistentPrefStore> overlay_;
};

TEST_F(OverlayPersistentPrefStoreTest, Observer) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  // Check that underlay first value is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(1);
  underlay_->SetValue(key, Value::CreateIntegerValue(42));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that underlay overwriting is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(1);
  underlay_->SetValue(key, Value::CreateIntegerValue(43));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that overwriting change in overlay is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(1);
  overlay_->SetValue(key, Value::CreateIntegerValue(44));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that hidden underlay change is not reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(0);
  underlay_->SetValue(key, Value::CreateIntegerValue(45));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that overlay remove is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(1);
  overlay_->RemoveValue(key);
  Mock::VerifyAndClearExpectations(&obs);

  // Check that underlay remove is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(1);
  underlay_->RemoveValue(key);
  Mock::VerifyAndClearExpectations(&obs);

  // Check respecting of silence.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(0);
  overlay_->SetValueSilently(key, Value::CreateIntegerValue(46));
  Mock::VerifyAndClearExpectations(&obs);

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(key))).Times(0);
  underlay_->SetValue(key, Value::CreateIntegerValue(47));
  overlay_->SetValue(key, Value::CreateIntegerValue(48));
  Mock::VerifyAndClearExpectations(&obs);
}

TEST_F(OverlayPersistentPrefStoreTest, GetAndSet) {
  Value* value = NULL;
  int i = -1;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, overlay_->GetValue(key, &value));
  EXPECT_EQ(PrefStore::READ_NO_VALUE, underlay_->GetValue(key, &value));

  underlay_->SetValue(key, Value::CreateIntegerValue(42));

  // Value shines through:
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  overlay_->SetValue(key, Value::CreateIntegerValue(43));

  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(43, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  overlay_->RemoveValue(key);

  // Value shines through:
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);
}
