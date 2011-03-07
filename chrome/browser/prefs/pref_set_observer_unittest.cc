// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for PrefSetObserver.
class PrefSetObserverTest : public testing::Test {
 public:
  virtual void SetUp() {
    pref_service_.reset(new TestingPrefService);
    pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com");
    pref_service_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, false);
    pref_service_->RegisterStringPref(prefs::kApplicationLocale, "");
  }

  PrefSetObserver* CreatePrefSetObserver(NotificationObserver* observer) {
    PrefSetObserver* pref_set =
        new PrefSetObserver(pref_service_.get(), observer);
    pref_set->AddPref(prefs::kHomePage);
    pref_set->AddPref(prefs::kHomePageIsNewTabPage);
    return pref_set;
  }

  scoped_ptr<TestingPrefService> pref_service_;
};

TEST_F(PrefSetObserverTest, IsObserved) {
  scoped_ptr<PrefSetObserver> pref_set(CreatePrefSetObserver(NULL));
  EXPECT_TRUE(pref_set->IsObserved(prefs::kHomePage));
  EXPECT_TRUE(pref_set->IsObserved(prefs::kHomePageIsNewTabPage));
  EXPECT_FALSE(pref_set->IsObserved(prefs::kApplicationLocale));
}

TEST_F(PrefSetObserverTest, IsManaged) {
  scoped_ptr<PrefSetObserver> pref_set(CreatePrefSetObserver(NULL));
  EXPECT_FALSE(pref_set->IsManaged());
  pref_service_->SetManagedPref(prefs::kHomePage,
                                Value::CreateStringValue("http://crbug.com"));
  EXPECT_TRUE(pref_set->IsManaged());
  pref_service_->SetManagedPref(prefs::kHomePageIsNewTabPage,
                                Value::CreateBooleanValue(true));
  EXPECT_TRUE(pref_set->IsManaged());
  pref_service_->RemoveManagedPref(prefs::kHomePage);
  EXPECT_TRUE(pref_set->IsManaged());
  pref_service_->RemoveManagedPref(prefs::kHomePageIsNewTabPage);
  EXPECT_FALSE(pref_set->IsManaged());
}

MATCHER_P(PrefNameDetails, name, "details references named preference") {
  std::string* pstr = reinterpret_cast<const Details<std::string>&>(arg).ptr();
  return pstr && *pstr == name;
}

TEST_F(PrefSetObserverTest, Observe) {
  using testing::_;
  using testing::Mock;

  NotificationObserverMock observer;
  scoped_ptr<PrefSetObserver> pref_set(CreatePrefSetObserver(&observer));

  EXPECT_CALL(observer,
              Observe(NotificationType(NotificationType::PREF_CHANGED),
                      Source<PrefService>(pref_service_.get()),
                      PrefNameDetails(prefs::kHomePage)));
  pref_service_->SetUserPref(prefs::kHomePage,
                             Value::CreateStringValue("http://crbug.com"));
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              Observe(NotificationType(NotificationType::PREF_CHANGED),
                      Source<PrefService>(pref_service_.get()),
                      PrefNameDetails(prefs::kHomePageIsNewTabPage)));
  pref_service_->SetUserPref(prefs::kHomePageIsNewTabPage,
                             Value::CreateBooleanValue(true));
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, Observe(_, _, _)).Times(0);
  pref_service_->SetUserPref(prefs::kApplicationLocale,
                             Value::CreateStringValue("en_US.utf8"));
  Mock::VerifyAndClearExpectations(&observer);
}
