// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_observer_mock.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/test/testing_pref_service.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Field;
using testing::Invoke;
using testing::Mock;
using testing::Truly;

namespace {

const char kChangedPref[] = "changed_pref";
const char kUnchangedPref[] = "unchanged_pref";

bool DetailsAreChangedPref(const Details<std::string>& details) {
  std::string* string_in = Details<std::string>(details).ptr();
  return strcmp(string_in->c_str(), kChangedPref) == 0;
}

// Test PrefNotifier that allows tracking of observers and notifications.
class MockPrefNotifier : public PrefNotifierImpl {
 public:
  explicit MockPrefNotifier(PrefService* pref_service)
      : PrefNotifierImpl(pref_service) {}
  virtual ~MockPrefNotifier() {}

  MOCK_METHOD1(FireObservers, void(const std::string& path));

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

// Test fixture class.
class PrefNotifierTest : public testing::Test {
 protected:
  virtual void SetUp() {
    pref_service_.RegisterBooleanPref(kChangedPref, true);
    pref_service_.RegisterBooleanPref(kUnchangedPref, true);
  }

  TestingPrefService pref_service_;

  PrefObserverMock obs1_;
  PrefObserverMock obs2_;
};

TEST_F(PrefNotifierTest, OnPreferenceChanged) {
  MockPrefNotifier notifier(&pref_service_);
  EXPECT_CALL(notifier, FireObservers(kChangedPref)).Times(1);
  notifier.OnPreferenceChanged(kChangedPref);
}

TEST_F(PrefNotifierTest, OnInitializationCompleted) {
  MockPrefNotifier notifier(&pref_service_);
  NotificationObserverMock observer;
  NotificationRegistrar registrar;
  registrar.Add(&observer, NotificationType::PREF_INITIALIZATION_COMPLETED,
                Source<PrefService>(&pref_service_));
  EXPECT_CALL(observer, Observe(
      Field(&NotificationType::value,
            NotificationType::PREF_INITIALIZATION_COMPLETED),
      Source<PrefService>(&pref_service_),
      NotificationService::NoDetails()));
  notifier.OnInitializationCompleted();
}

TEST_F(PrefNotifierTest, AddAndRemovePrefObservers) {
  const char pref_name[] = "homepage";
  const char pref_name2[] = "proxy";

  MockPrefNotifier notifier(&pref_service_);
  notifier.AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Re-adding the same observer for the same pref doesn't change anything.
  // Skip this in debug mode, since it hits a DCHECK and death tests aren't
  // thread-safe.
#if defined(NDEBUG)
  notifier.AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));
#endif  // NDEBUG

  // Ensure that we can add the same observer to a different pref.
  notifier.AddPrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Ensure that we can add another observer to the same pref.
  notifier.AddPrefObserver(pref_name, &obs2_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Ensure that we can remove all observers, and that removing a non-existent
  // observer is harmless.
  notifier.RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name, &obs2_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));
}

TEST_F(PrefNotifierTest, FireObservers) {
  FundamentalValue value_true(true);
  PrefNotifierImpl notifier(&pref_service_);
  notifier.AddPrefObserver(kChangedPref, &obs1_);
  notifier.AddPrefObserver(kUnchangedPref, &obs1_);

  obs1_.Expect(&pref_service_, kChangedPref, &value_true);
  EXPECT_CALL(obs2_, Observe(_, _, _)).Times(0);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  notifier.AddPrefObserver(kChangedPref, &obs2_);
  notifier.AddPrefObserver(kUnchangedPref, &obs2_);

  obs1_.Expect(&pref_service_, kChangedPref, &value_true);
  obs2_.Expect(&pref_service_, kChangedPref, &value_true);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer from one pref doesn't affect anything else.
  notifier.RemovePrefObserver(kChangedPref, &obs1_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  obs2_.Expect(&pref_service_, kChangedPref, &value_true);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer entirely doesn't affect anything else.
  notifier.RemovePrefObserver(kUnchangedPref, &obs1_);

  EXPECT_CALL(obs1_, Observe(_, _, _)).Times(0);
  obs2_.Expect(&pref_service_, kChangedPref, &value_true);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  notifier.RemovePrefObserver(kChangedPref, &obs2_);
  notifier.RemovePrefObserver(kUnchangedPref, &obs2_);
}

}  // namespace
