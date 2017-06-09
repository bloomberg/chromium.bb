// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/overlay_user_pref_store.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store_unittest.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::StrEq;

namespace base {
namespace {

const char kBrowserWindowPlacement[] = "browser.window_placement";
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";
const char kSharedKey[] = "sync_promo.show_on_first_run_allowed";

const char* const overlay_key = kBrowserWindowPlacement;
const char* const regular_key = kShowBookmarkBar;
const char* const shared_key = kSharedKey;

}  // namespace

class OverlayUserPrefStoreTest : public testing::Test {
 protected:
  OverlayUserPrefStoreTest()
      : underlay_(new TestingPrefStore()),
        overlay_(new OverlayUserPrefStore(underlay_.get())) {
    overlay_->RegisterOverlayPref(overlay_key);
    overlay_->RegisterOverlayPref(shared_key);
  }

  ~OverlayUserPrefStoreTest() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<TestingPrefStore> underlay_;
  scoped_refptr<OverlayUserPrefStore> overlay_;
};

TEST_F(OverlayUserPrefStoreTest, Observer) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  // Check that underlay first value is reported.
  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(overlay_key);

  // Check that underlay overwriting is reported.
  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(43),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(overlay_key);

  // Check that overwriting change in overlay is reported.
  overlay_->SetValue(overlay_key, base::MakeUnique<Value>(44),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(overlay_key);

  // Check that hidden underlay change is not reported.
  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(45),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  // Check that overlay remove is reported.
  overlay_->RemoveValue(overlay_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(overlay_key);

  // Check that underlay remove is reported.
  underlay_->RemoveValue(overlay_key,
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(overlay_key);

  // Check respecting of silence.
  overlay_->SetValueSilently(overlay_key, base::MakeUnique<Value>(46),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(47),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(overlay_key, base::MakeUnique<Value>(48),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());
}

TEST_F(OverlayUserPrefStoreTest, GetAndSet) {
  const Value* value = NULL;
  EXPECT_FALSE(overlay_->GetValue(overlay_key, &value));
  EXPECT_FALSE(underlay_->GetValue(overlay_key, &value));

  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Value shines through:
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  overlay_->SetValue(overlay_key, base::MakeUnique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  overlay_->RemoveValue(overlay_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Value shines through:
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

// Check that GetMutableValue does not return the dictionary of the underlay.
TEST_F(OverlayUserPrefStoreTest, ModifyDictionaries) {
  underlay_->SetValue(overlay_key, base::MakeUnique<DictionaryValue>(),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  Value* modify = NULL;
  EXPECT_TRUE(overlay_->GetMutableValue(overlay_key, &modify));
  ASSERT_TRUE(modify);
  ASSERT_TRUE(modify->IsType(Value::Type::DICTIONARY));
  static_cast<DictionaryValue*>(modify)->SetInteger(overlay_key, 42);

  Value* original_in_underlay = NULL;
  EXPECT_TRUE(underlay_->GetMutableValue(overlay_key, &original_in_underlay));
  ASSERT_TRUE(original_in_underlay);
  ASSERT_TRUE(original_in_underlay->IsType(Value::Type::DICTIONARY));
  EXPECT_TRUE(static_cast<DictionaryValue*>(original_in_underlay)->empty());

  Value* modified = NULL;
  EXPECT_TRUE(overlay_->GetMutableValue(overlay_key, &modified));
  ASSERT_TRUE(modified);
  ASSERT_TRUE(modified->IsType(Value::Type::DICTIONARY));
  EXPECT_TRUE(Value::Equals(modify, static_cast<DictionaryValue*>(modified)));
}

// Here we consider a global preference that is not overlayed.
TEST_F(OverlayUserPrefStoreTest, GlobalPref) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  const Value* value = NULL;

  // Check that underlay first value is reported.
  underlay_->SetValue(regular_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that underlay overwriting is reported.
  underlay_->SetValue(regular_key, base::MakeUnique<Value>(43),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that we get this value from the overlay
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  // Check that overwriting change in overlay is reported.
  overlay_->SetValue(regular_key, base::MakeUnique<Value>(44),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that we get this value from the overlay and the underlay.
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(44).Equals(value));
  EXPECT_TRUE(underlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(44).Equals(value));

  // Check that overlay remove is reported.
  overlay_->RemoveValue(regular_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that value was removed from overlay and underlay
  EXPECT_FALSE(overlay_->GetValue(regular_key, &value));
  EXPECT_FALSE(underlay_->GetValue(regular_key, &value));

  // Check respecting of silence.
  overlay_->SetValueSilently(regular_key, base::MakeUnique<Value>(46),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  underlay_->SetValue(regular_key, base::MakeUnique<Value>(47),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(regular_key, base::MakeUnique<Value>(48),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());
}

// Check that mutable values are removed correctly.
TEST_F(OverlayUserPrefStoreTest, ClearMutableValues) {
  // Set in overlay and underlay the same preference.
  underlay_->SetValue(overlay_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(overlay_key, base::MakeUnique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
  overlay_->ClearMutableValues();

  // Check that an underlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

// Check that mutable values are removed correctly when using a silent set.
TEST_F(OverlayUserPrefStoreTest, ClearMutableValues_Silently) {
  // Set in overlay and underlay the same preference.
  underlay_->SetValueSilently(overlay_key, base::MakeUnique<Value>(42),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValueSilently(overlay_key, base::MakeUnique<Value>(43),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
  overlay_->ClearMutableValues();

  // Check that an underlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(overlay_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

TEST_F(OverlayUserPrefStoreTest, GetValues) {
  // To check merge behavior, create underlay and overlay so each has a key the
  // other doesn't have and they have one key in common.
  underlay_->SetValue(regular_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(overlay_key, base::MakeUnique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  underlay_->SetValue(shared_key, base::MakeUnique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(shared_key, base::MakeUnique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  auto values = overlay_->GetValues();
  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  ASSERT_TRUE(values->Get(overlay_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  // Check that an underlay preference is returned.
  ASSERT_TRUE(values->Get(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  // Check that the overlay is preferred.
  ASSERT_TRUE(values->Get(shared_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
}

TEST_F(OverlayUserPrefStoreTest, CommitPendingWriteWithCallback) {
  TestCommitPendingWriteWithCallback(overlay_.get());
}

}  // namespace base
