// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/prefs/incognito_user_pref_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_store_observer_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::StrEq;

namespace {

const char* incognito_key = prefs::kBrowserWindowPlacement;
const char* regular_key = prefs::kShowBookmarkBar;

}  // namespace

class IncognitoUserPrefStoreTest : public testing::Test {
 public:
  IncognitoUserPrefStoreTest()
      : underlay_(new TestingPrefStore()),
        overlay_(new IncognitoUserPrefStore(underlay_.get())) {
  }

  virtual ~IncognitoUserPrefStoreTest() {}

  scoped_refptr<TestingPrefStore> underlay_;
  scoped_refptr<IncognitoUserPrefStore> overlay_;
};

TEST_F(IncognitoUserPrefStoreTest, Observer) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  // Check that underlay first value is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(1);
  underlay_->SetValue(incognito_key, Value::CreateIntegerValue(42));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that underlay overwriting is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(1);
  underlay_->SetValue(incognito_key, Value::CreateIntegerValue(43));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that overwriting change in overlay is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(1);
  overlay_->SetValue(incognito_key, Value::CreateIntegerValue(44));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that hidden underlay change is not reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(0);
  underlay_->SetValue(incognito_key, Value::CreateIntegerValue(45));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that overlay remove is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(1);
  overlay_->RemoveValue(incognito_key);
  Mock::VerifyAndClearExpectations(&obs);

  // Check that underlay remove is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(1);
  underlay_->RemoveValue(incognito_key);
  Mock::VerifyAndClearExpectations(&obs);

  // Check respecting of silence.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(0);
  overlay_->SetValueSilently(incognito_key, Value::CreateIntegerValue(46));
  Mock::VerifyAndClearExpectations(&obs);

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(incognito_key))).Times(0);
  underlay_->SetValue(incognito_key, Value::CreateIntegerValue(47));
  overlay_->SetValue(incognito_key, Value::CreateIntegerValue(48));
  Mock::VerifyAndClearExpectations(&obs);
}

TEST_F(IncognitoUserPrefStoreTest, GetAndSet) {
  const Value* value = NULL;
  int i = -1;
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            overlay_->GetValue(incognito_key, &value));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            underlay_->GetValue(incognito_key, &value));

  underlay_->SetValue(incognito_key, Value::CreateIntegerValue(42));

  // Value shines through:
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  overlay_->SetValue(incognito_key, Value::CreateIntegerValue(43));

  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(43, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  overlay_->RemoveValue(incognito_key);

  // Value shines through:
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);

  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(incognito_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(42, i);
}

// Check that GetMutableValue does not return the dictionary of the underlay.
TEST_F(IncognitoUserPrefStoreTest, ModifyDictionaries) {
  underlay_->SetValue(incognito_key, new DictionaryValue);

  Value* modify = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            overlay_->GetMutableValue(incognito_key, &modify));
  ASSERT_TRUE(modify);
  ASSERT_TRUE(modify->GetType() == Value::TYPE_DICTIONARY);
  static_cast<DictionaryValue*>(modify)->SetInteger(incognito_key, 42);

  Value* original_in_underlay = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            underlay_->GetMutableValue(incognito_key, &original_in_underlay));
  ASSERT_TRUE(original_in_underlay);
  ASSERT_TRUE(original_in_underlay->GetType() == Value::TYPE_DICTIONARY);
  EXPECT_TRUE(static_cast<DictionaryValue*>(original_in_underlay)->empty());

  Value* modified = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            overlay_->GetMutableValue(incognito_key, &modified));
  ASSERT_TRUE(modified);
  ASSERT_TRUE(modified->GetType() == Value::TYPE_DICTIONARY);
  EXPECT_TRUE(Value::Equals(modify, static_cast<DictionaryValue*>(modified)));
}

// Here we consider a global preference that is not incognito specific.
TEST_F(IncognitoUserPrefStoreTest, GlobalPref) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  const Value* value = NULL;
  int i = -1;

  // Check that underlay first value is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(1);
  underlay_->SetValue(regular_key, Value::CreateIntegerValue(42));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that underlay overwriting is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(1);
  underlay_->SetValue(regular_key, Value::CreateIntegerValue(43));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that we get this value from the overlay
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(regular_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(43, i);

  // Check that overwriting change in overlay is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(1);
  overlay_->SetValue(regular_key, Value::CreateIntegerValue(44));
  Mock::VerifyAndClearExpectations(&obs);

  // Check that we get this value from the overlay and the underlay.
  EXPECT_EQ(PrefStore::READ_OK, overlay_->GetValue(regular_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(44, i);
  EXPECT_EQ(PrefStore::READ_OK, underlay_->GetValue(regular_key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&i));
  EXPECT_EQ(44, i);

  // Check that overlay remove is reported.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(1);
  overlay_->RemoveValue(regular_key);
  Mock::VerifyAndClearExpectations(&obs);

  // Check that value was removed from overlay and underlay
  EXPECT_EQ(PrefStore::READ_NO_VALUE, overlay_->GetValue(regular_key, &value));
  EXPECT_EQ(PrefStore::READ_NO_VALUE, underlay_->GetValue(regular_key, &value));

  // Check respecting of silence.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(0);
  overlay_->SetValueSilently(regular_key, Value::CreateIntegerValue(46));
  Mock::VerifyAndClearExpectations(&obs);

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  EXPECT_CALL(obs, OnPrefValueChanged(StrEq(regular_key))).Times(0);
  underlay_->SetValue(regular_key, Value::CreateIntegerValue(47));
  overlay_->SetValue(regular_key, Value::CreateIntegerValue(48));
  Mock::VerifyAndClearExpectations(&obs);
}
