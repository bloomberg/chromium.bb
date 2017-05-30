// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeWebDataService : public autofill::AutofillWebDataService {
 public:
  FakeWebDataService()
      : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()) {}

 protected:
  ~FakeWebDataService() override {}
};

}  // namespace

class BrowsingDataUtilsTest : public testing::Test {
 public:
  BrowsingDataUtilsTest() {}
  ~BrowsingDataUtilsTest() override {}

  void SetUp() override {
    browsing_data::prefs::RegisterBrowserUserPrefs(prefs_.registry());
  }

  PrefService* prefs() { return &prefs_; }

 private:
  base::MessageLoop loop_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

// Tests the complex output of the Autofill counter.
TEST_F(BrowsingDataUtilsTest, AutofillCounterResult) {
  browsing_data::AutofillCounter counter(
      scoped_refptr<FakeWebDataService>(new FakeWebDataService()), nullptr);

  // Test all configurations of zero and nonzero partial results for datatypes.
  // Test singular and plural for each datatype.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, 0, false, "None"},
      {0, 0, 0, true, "None"},
      {1, 0, 0, false, "1 credit card"},
      {0, 5, 0, false, "5 addresses"},
      {0, 0, 1, false, "1 suggestion"},
      {0, 0, 2, false, "2 suggestions"},
      {0, 0, 2, true, "2 suggestions (synced)"},
      {4, 7, 0, false, "4 credit cards, 7 addresses"},
      {4, 7, 0, true, "4 credit cards, 7 addresses (synced)"},
      {3, 0, 9, false, "3 credit cards, 9 other suggestions"},
      {0, 1, 1, false, "1 address, 1 other suggestion"},
      {9, 6, 3, false, "9 credit cards, 6 addresses, 3 others"},
      {4, 2, 1, false, "4 credit cards, 2 addresses, 1 other"},
      {4, 2, 1, true, "4 credit cards, 2 addresses, 1 other (synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    browsing_data::AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses, test_case.sync_enabled);

    SCOPED_TRACE(
        base::StringPrintf("Test params: %d credit card(s), "
                           "%d address(es), %d suggestion(s).",
                           test_case.num_credit_cards, test_case.num_addresses,
                           test_case.num_suggestions));

    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

// Tests the output of the Passwords counter.
TEST_F(BrowsingDataUtilsTest, PasswordsCounterResult) {
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore());
  browsing_data::PasswordsCounter counter(
      scoped_refptr<password_manager::PasswordStore>(store), nullptr);

  const struct TestCase {
    int num_passwords;
    int is_synced;
    std::string expected_output;
  } kTestCases[] = {
      {0, false, "None"},        {0, true, "None"},
      {1, false, "1 password"},  {1, true, "1 password (synced)"},
      {5, false, "5 passwords"}, {5, true, "5 passwords (synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::SyncResult result(
        &counter, test_case.num_passwords, test_case.is_synced);
    SCOPED_TRACE(base::StringPrintf("Test params: %d password(s), %d is_synced",
                                    test_case.num_passwords,
                                    test_case.is_synced));
    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
  store->ShutdownOnUIThread();
}

TEST_F(BrowsingDataUtilsTest, MigratePreferencesToBasic) {
  using namespace browsing_data::prefs;

  prefs()->SetBoolean(kDeleteBrowsingHistory, true);
  prefs()->SetBoolean(kDeleteCookies, false);
  prefs()->SetBoolean(kDeleteCache, false);
  prefs()->SetInteger(kDeleteTimePeriod, 42);

  // History, cookies and cache should be migrated to their basic counterpart.
  browsing_data::MigratePreferencesToBasic(prefs());
  EXPECT_TRUE(prefs()->GetBoolean(kDeleteBrowsingHistoryBasic));
  EXPECT_FALSE(prefs()->GetBoolean(kDeleteCookiesBasic));
  EXPECT_FALSE(prefs()->GetBoolean(kDeleteCacheBasic));
  EXPECT_EQ(42, prefs()->GetInteger(kDeleteTimePeriodBasic));

  prefs()->SetBoolean(kDeleteBrowsingHistory, true);
  prefs()->SetBoolean(kDeleteCookies, true);
  prefs()->SetBoolean(kDeleteCache, true);
  prefs()->SetInteger(kDeleteTimePeriod, 100);

  // After the first migration all settings should stay the same if the
  // migration is executed again.
  browsing_data::MigratePreferencesToBasic(prefs());
  EXPECT_TRUE(prefs()->GetBoolean(kDeleteBrowsingHistoryBasic));
  EXPECT_FALSE(prefs()->GetBoolean(kDeleteCookiesBasic));
  EXPECT_FALSE(prefs()->GetBoolean(kDeleteCacheBasic));
  EXPECT_EQ(42, prefs()->GetInteger(kDeleteTimePeriodBasic));
}
