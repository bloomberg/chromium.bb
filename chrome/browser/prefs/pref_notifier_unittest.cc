// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

const char kChangedPref[] = "changed_pref";
const char kUnchangedPref[] = "unchanged_pref";

bool DetailsAreChangedPref(const Details<std::string>& details) {
  std::string* string_in = Details<std::string>(details).ptr();
  return strcmp(string_in->c_str(), kChangedPref) == 0;
}

// Test PrefNotifier that allows tracking of observers and notifications.
class MockPrefNotifier : public PrefNotifier {
 public:
  MockPrefNotifier(PrefService* prefs, PrefValueStore* value_store)
      : PrefNotifier(prefs, value_store) {}

  virtual ~MockPrefNotifier() {}

  MOCK_METHOD1(FireObservers, void(const char* path));

  void RealFireObservers(const char* path) {
    PrefNotifier::FireObservers(path);
  }

  size_t CountObserver(const char* path, NotificationObserver* obs) {
    PrefObserverMap::const_iterator observer_iterator =
        pref_observers()->find(path);
    if (observer_iterator == pref_observers()->end())
      return false;

    NotificationObserverList* observer_list = observer_iterator->second;
    NotificationObserverList::Iterator it(*observer_list);
    NotificationObserver* existing_obs;
    size_t count = 0;
    while ((existing_obs = it.GetNext()) != NULL) {
      if (existing_obs == obs)
        count++;
    }

    return count;
  }
};

// Mock PrefValueStore that has no unnecessary PrefStores and injects a simpler
// PrefHasChanged().
class MockPrefValueStore : public PrefValueStore {
 public:
  MockPrefValueStore()
      : PrefValueStore(NULL, NULL, NULL, NULL, NULL, NULL,
                       new DefaultPrefStore()) {}

  virtual ~MockPrefValueStore() {}

  // This mock version returns true if the pref path starts with "changed".
  virtual bool PrefHasChanged(const char* path,
                              PrefNotifier::PrefStoreType new_store) {
    std::string path_str(path);
    if (path_str.compare(0, 7, "changed") == 0)
      return true;
    return false;
  }
};

// Mock PrefService that allows the PrefNotifier to be injected.
class MockPrefService : public PrefService {
 public:
  explicit MockPrefService(PrefValueStore* pref_value_store)
      : PrefService(pref_value_store) {}

  void SetPrefNotifier(PrefNotifier* notifier) {
    pref_notifier_.reset(notifier);
  }
};

// Mock PrefObserver that verifies notifications.
class MockPrefObserver : public NotificationObserver {
 public:
  virtual ~MockPrefObserver() {}

  MOCK_METHOD3(Observe, void(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details));
};

// Test fixture class.
class PrefNotifierTest : public testing::Test {
 protected:
  virtual void SetUp() {
    value_store_ = new MockPrefValueStore;
    pref_service_.reset(new MockPrefService(value_store_));
    notifier_ = new MockPrefNotifier(pref_service_.get(), value_store_);
    pref_service_->SetPrefNotifier(notifier_);

    pref_service_->RegisterBooleanPref(kChangedPref, true);
    pref_service_->RegisterBooleanPref(kUnchangedPref, true);
  }

  // The PrefService takes ownership of the PrefValueStore and PrefNotifier.
  PrefValueStore* value_store_;
  MockPrefNotifier* notifier_;
  scoped_ptr<MockPrefService> pref_service_;

  MockPrefObserver obs1_;
  MockPrefObserver obs2_;
};

TEST_F(PrefNotifierTest, OnPreferenceSet) {
  EXPECT_CALL(*notifier_, FireObservers(kChangedPref))
      .Times(PrefNotifier::PREF_STORE_TYPE_MAX + 1);
  EXPECT_CALL(*notifier_, FireObservers(kUnchangedPref)).Times(0);

  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    notifier_->OnPreferenceSet(kChangedPref,
        static_cast<PrefNotifier::PrefStoreType>(i));
    notifier_->OnPreferenceSet(kUnchangedPref,
        static_cast<PrefNotifier::PrefStoreType>(i));
  }
}

TEST_F(PrefNotifierTest, OnUserPreferenceSet) {
  EXPECT_CALL(*notifier_, FireObservers(kChangedPref));
  EXPECT_CALL(*notifier_, FireObservers(kUnchangedPref)).Times(0);
  notifier_->OnUserPreferenceSet(kChangedPref);
  notifier_->OnUserPreferenceSet(kUnchangedPref);
}

TEST_F(PrefNotifierTest, AddAndRemovePrefObservers) {
  const char pref_name[] = "homepage";
  const char pref_name2[] = "proxy";

  notifier_->AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  // Re-adding the same observer for the same pref doesn't change anything.
  // Skip this in debug mode, since it hits a DCHECK and death tests aren't
  // thread-safe.
#if defined(NDEBUG)
  notifier_->AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));
#endif  // NDEBUG

  // Ensure that we can add the same observer to a different pref.
  notifier_->AddPrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  // Ensure that we can add another observer to the same pref.
  notifier_->AddPrefObserver(pref_name, &obs2_);
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  // Ensure that we can remove all observers, and that removing a non-existent
  // observer is harmless.
  notifier_->RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  notifier_->RemovePrefObserver(pref_name, &obs2_);
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  notifier_->RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));

  notifier_->RemovePrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier_->CountObserver(pref_name2, &obs2_));
}

TEST_F(PrefNotifierTest, FireObservers) {
  using testing::_;
  using testing::Field;
  using testing::Invoke;
  using testing::Mock;
  using testing::Truly;

  // Tell googlemock to pass calls to the mock's "back door" too.
  ON_CALL(*notifier_, FireObservers(_))
      .WillByDefault(Invoke(notifier_, &MockPrefNotifier::RealFireObservers));
  EXPECT_CALL(*notifier_, FireObservers(kChangedPref)).Times(4);
  EXPECT_CALL(*notifier_, FireObservers(kUnchangedPref)).Times(0);

  notifier_->AddPrefObserver(kChangedPref, &obs1_);
  notifier_->AddPrefObserver(kUnchangedPref, &obs1_);

  EXPECT_CALL(obs1_, Observe(
      Field(&NotificationType::value, NotificationType::PREF_CHANGED),
      Source<PrefService>(pref_service_.get()),
      Truly(DetailsAreChangedPref)));
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier_->OnUserPreferenceSet(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier_->OnUserPreferenceSet(kUnchangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  notifier_->AddPrefObserver(kChangedPref, &obs2_);
  notifier_->AddPrefObserver(kUnchangedPref, &obs2_);

  EXPECT_CALL(obs1_, Observe(
      Field(&NotificationType::value, NotificationType::PREF_CHANGED),
      Source<PrefService>(pref_service_.get()),
      Truly(DetailsAreChangedPref)));
  EXPECT_CALL(obs2_, Observe(
      Field(&NotificationType::value, NotificationType::PREF_CHANGED),
      Source<PrefService>(pref_service_.get()),
      Truly(DetailsAreChangedPref)));
  notifier_->OnUserPreferenceSet(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier_->OnUserPreferenceSet(kUnchangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer from one pref doesn't affect anything else.
  notifier_->RemovePrefObserver(kChangedPref, &obs1_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(
      Field(&NotificationType::value, NotificationType::PREF_CHANGED),
      Source<PrefService>(pref_service_.get()),
      Truly(DetailsAreChangedPref)));
  notifier_->OnUserPreferenceSet(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier_->OnUserPreferenceSet(kUnchangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer entirely doesn't affect anything else.
  notifier_->RemovePrefObserver(kUnchangedPref, &obs1_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(
      Field(&NotificationType::value, NotificationType::PREF_CHANGED),
      Source<PrefService>(pref_service_.get()),
      Truly(DetailsAreChangedPref)));
  notifier_->OnUserPreferenceSet(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier_->OnUserPreferenceSet(kUnchangedPref);

  notifier_->RemovePrefObserver(kChangedPref, &obs2_);
  notifier_->RemovePrefObserver(kUnchangedPref, &obs2_);
}

}  // namespace
