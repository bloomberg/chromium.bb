// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/rlz.h"

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/path_service.h"
#include "base/test/test_reg_util_win.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using registry_util::RegistryOverrideManager;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::AssertionFailure;

namespace {

// Registry path to overridden hive.
const wchar_t kRlzTempHkcu[] = L"rlz_hkcu";
const wchar_t kRlzTempHklm[] = L"rlz_hklm";

// Dummy RLZ string for the access points.
const char kOmniboxRlzString[] = "test_omnibox";
const char kHomepageRlzString[] = "test_homepage";
const char kNewOmniboxRlzString[] = "new_omnibox";
const char kNewHomepageRlzString[] = "new_homepage";

// Some helper macros to test it a string contains/does not contain a substring.

AssertionResult CmpHelperSTRC(const char* str_expression,
                              const char* substr_expression,
                              const char* str,
                              const char* substr) {
  if (NULL != strstr(str, substr)) {
    return AssertionSuccess();
  }

  return AssertionFailure() << "Expected: (" << substr_expression << ") in ("
                            << str_expression << "), actual: '"
                            << substr << "' not in '" << str << "'";
}

AssertionResult CmpHelperSTRNC(const char* str_expression,
                               const char* substr_expression,
                               const char* str,
                               const char* substr) {
  if (NULL == strstr(str, substr)) {
    return AssertionSuccess();
  }

  return AssertionFailure() << "Expected: (" << substr_expression
                            << ") not in (" << str_expression << "), actual: '"
                            << substr << "' in '" << str << "'";
}

#define EXPECT_STR_CONTAINS(str, substr) \
    EXPECT_PRED_FORMAT2(CmpHelperSTRC, str, substr)

#define EXPECT_STR_NOT_CONTAIN(str, substr) \
    EXPECT_PRED_FORMAT2(CmpHelperSTRNC, str, substr)

}  // namespace

// Test class for RLZ tracker. Makes some member functions public and
// overrides others to make it easier to test.
class TestRLZTracker : public RLZTracker {
 public:
  using RLZTracker::DelayedInit;
  using RLZTracker::Observe;

  TestRLZTracker() : assume_not_ui_thread_(false) {
    set_tracker(this);
  }

  virtual ~TestRLZTracker() {
    set_tracker(NULL);
  }

  bool was_ping_sent_for_brand(const std::string& brand) const {
    return pinged_brands_.count(brand) > 0;
  }

  void set_assume_not_ui_thread(bool assume_not_ui_thread) {
    assume_not_ui_thread_ = assume_not_ui_thread;
  }

 private:
  virtual void ScheduleDelayedInit(int delay) OVERRIDE {
    // If the delay is 0, invoke the delayed init now. Otherwise,
    // don't schedule anything, it will be manually called during tests.
    if (delay == 0)
      DelayedInit();
  }

  virtual void ScheduleFinancialPing() OVERRIDE {
    PingNow(this);
  }

  virtual bool ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) OVERRIDE {
    return !assume_not_ui_thread_;
  }

  virtual bool SendFinancialPing(const std::string& brand,
                                 const string16& lang,
                                 const string16& referral) OVERRIDE {
    // Don't ping the server during tests, just pretend as if we did.
    EXPECT_FALSE(brand.empty());
    pinged_brands_.insert(brand);

    // Set new access points RLZ string, like the actual server ping would have
    // done.
    rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kNewOmniboxRlzString);
    rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE,
                               kNewHomepageRlzString);
    return true;
  }

  std::set<std::string> pinged_brands_;
  bool assume_not_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestRLZTracker);
};

class RlzLibTest : public testing::Test {
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  void SetMainBrand(const char* brand);
  void SetReactivationBrand(const char* brand);
  void SetRegistryBrandValue(const wchar_t* name, const char* brand);

  void SimulateOmniboxUsage();
  void SimulateHomepageUsage();
  void InvokeDelayedInit();

  void ExpectEventRecorded(const char* event_name, bool expected);
  void ExpectRlzPingSent(bool expected);
  void ExpectReactivationRlzPingSent(bool expected);

  TestRLZTracker tracker_;
  RegistryOverrideManager override_manager_;
};

void RlzLibTest::SetUp() {
  testing::Test::SetUp();

  // Before overriding HKLM for the tests, we need to set it up correctly
  // so that the rlz_lib calls work. This needs to be done before we do the
  // override.

  string16 temp_hklm_path = base::StringPrintf(
      L"%ls\\%ls",
      RegistryOverrideManager::kTempTestKeyPath,
      kRlzTempHklm);

  base::win::RegKey hklm;
  ASSERT_EQ(ERROR_SUCCESS, hklm.Create(HKEY_CURRENT_USER,
                                       temp_hklm_path.c_str(),
                                       KEY_READ));

  string16 temp_hkcu_path = base::StringPrintf(
      L"%ls\\%ls",
      RegistryOverrideManager::kTempTestKeyPath,
      kRlzTempHkcu);

  base::win::RegKey hkcu;
  ASSERT_EQ(ERROR_SUCCESS, hkcu.Create(HKEY_CURRENT_USER,
                                       temp_hkcu_path.c_str(),
                                       KEY_READ));

  rlz_lib::InitializeTempHivesForTesting(hklm, hkcu);

  // Its important to override HKLM before HKCU because of the registry
  // initialization performed above.
  override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, kRlzTempHklm);
  override_manager_.OverrideRegistry(HKEY_CURRENT_USER, kRlzTempHkcu);

  // Make sure a non-organic brand code is set in the registry or the RLZTracker
  // is pretty much a no-op.
  SetMainBrand("TEST");
  SetReactivationBrand("");
}

void RlzLibTest::TearDown() {
  testing::Test::TearDown();
}

void RlzLibTest::SetMainBrand(const char* brand) {
  SetRegistryBrandValue(google_update::kRegRLZBrandField, brand);
  std::string check_brand;
  google_util::GetBrand(&check_brand);
  EXPECT_EQ(brand, check_brand);
}

void RlzLibTest::SetReactivationBrand(const char* brand) {
  SetRegistryBrandValue(google_update::kRegRLZReactivationBrandField, brand);
  std::string check_brand;
  google_util::GetReactivationBrand(&check_brand);
  EXPECT_EQ(brand, check_brand);
}

void RlzLibTest::SetRegistryBrandValue(const wchar_t* name,
                                       const char* brand) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_SET_VALUE);
  if (*brand == 0) {
    LONG result = key.DeleteValue(name);
    ASSERT_TRUE(ERROR_SUCCESS == result || ERROR_FILE_NOT_FOUND == result);
  } else {
    string16 brand16 = ASCIIToWide(brand);
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name, brand16.c_str()));
  }
}

void RlzLibTest::SimulateOmniboxUsage() {
  tracker_.Observe(chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                   content::NotificationService::AllSources(),
                   content::Details<AutocompleteLog>(NULL));
}

void RlzLibTest::SimulateHomepageUsage() {
  NavigationEntry entry(NULL, 0, GURL(), GURL(), string16(),
                        content::PAGE_TRANSITION_HOME_PAGE, false);
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(&entry));
}

void RlzLibTest::InvokeDelayedInit() {
  tracker_.DelayedInit();
}

void RlzLibTest::ExpectEventRecorded(const char* event_name, bool expected) {
  char cgi[rlz_lib::kMaxCgiLength];
  GetProductEventsAsCgi(rlz_lib::CHROME, cgi, arraysize(cgi));
  if (expected) {
    EXPECT_STR_CONTAINS(cgi, event_name);
  } else {
    EXPECT_STR_NOT_CONTAIN(cgi, event_name);
  }
}

void RlzLibTest::ExpectRlzPingSent(bool expected) {
  std::string brand;
  google_util::GetBrand(&brand);
  EXPECT_EQ(expected, tracker_.was_ping_sent_for_brand(brand.c_str()));
}

void RlzLibTest::ExpectReactivationRlzPingSent(bool expected) {
  std::string brand;
  google_util::GetReactivationBrand(&brand);
  EXPECT_EQ(expected, tracker_.was_ping_sent_for_brand(brand.c_str()));
}

TEST_F(RlzLibTest, RecordProductEvent) {
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, rlz_lib::CHROME_OMNIBOX,
                                 rlz_lib::FIRST_SEARCH);

  ExpectEventRecorded("C1F", true);
}

// The events that affect the different RLZ scenarios are the following:
//
//  A: the user starts chrome for the first time
//  B: the user stops chrome
//  C: the user start a subsequent time
//  D: the user stops chrome again
//  I: the RLZTracker::DelayedInit() method is invoked
//  X: the user performs a search using the omnibox
//  Y: the user performs a search using the home page
//
// The events A to D happen in chronological order, but the other events
// may happen at any point between A-B or C-D, in no particular order.
//
// The visible results of the scenarios are:
//
//  C1I event is recorded
//  C2I event is recorded
//  C1F event is recorded
//  C2F event is recorded
//  C1S event is recorded
//  C2S event is recorded
//  RLZ ping sent
//
// Variations on the above scenarios:
//
//  - if the delay specified to InitRlzDelayed() is negative, then the RLZ
//    ping should be sent out at the time of event X and not wait for I
//
// Also want to test that pre-warming the RLZ string cache works correctly.

TEST_F(RlzLibTest, QuickStopAfterStart) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);

  // Omnibox events.
  ExpectEventRecorded("C1I", false);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", false);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, DelayedInitOnly) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", true);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", true);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRunNoRlzStrings) {
  RLZTracker::InitRlzDelayed(false, 20, true, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", true);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", true);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRun) {
  // Set some dummy RLZ strings to simulate that we already ran before and
  // performed a successful ping to the RLZ server.
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kOmniboxRlzString);
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, kHomepageRlzString);

  RLZTracker::InitRlzDelayed(false, 20, true, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoGoogleDefaultSearchOrHomepage) {
  RLZTracker::InitRlzDelayed(true, 20, false, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageOnly) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded("C1I", false);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", true);

  // Home page events.
  ExpectEventRecorded("C2I", false);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, HomepageUsageOnly) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded("C1I", false);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", false);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", true);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, UsageBeforeDelayedInit) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  SimulateOmniboxUsage();
  SimulateHomepageUsage();
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", true);
  ExpectEventRecorded("C1F", true);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", true);
  ExpectEventRecorded("C2F", true);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageAfterDelayedInit) {
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();
  SimulateOmniboxUsage();
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", true);
  ExpectEventRecorded("C1F", true);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", true);
  ExpectEventRecorded("C2F", true);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageSendsPingWhenDelayNegative) {
  RLZTracker::InitRlzDelayed(true, -20, true, true);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded("C1I", true);
  ExpectEventRecorded("C1S", true);
  ExpectEventRecorded("C1F", true);

  // Home page events.
  ExpectEventRecorded("C2I", true);
  ExpectEventRecorded("C2S", true);
  ExpectEventRecorded("C2F", false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, HomepageUsageDoesNotSendPingWhenDelayNegative) {
  RLZTracker::InitRlzDelayed(true, -20, true, true);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded("C1I", false);
  ExpectEventRecorded("C1S", false);
  ExpectEventRecorded("C1F", false);

  // Home page events.
  ExpectEventRecorded("C2I", false);
  ExpectEventRecorded("C2S", false);
  ExpectEventRecorded("C2F", true);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, GetAccessPointRlzOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kOmniboxRlzString);

  string16 rlz;

  tracker_.set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kOmniboxRlzString, WideToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, GetAccessPointRlzNotOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kOmniboxRlzString);

  string16 rlz;

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_FALSE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
}

TEST_F(RlzLibTest, GetAccessPointRlzIsCached) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kOmniboxRlzString);

  string16 rlz;

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_FALSE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));

  tracker_.set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kOmniboxRlzString, WideToUTF8(rlz).c_str());

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kOmniboxRlzString, WideToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, PingUpdatesRlzCache) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, kOmniboxRlzString);
  rlz_lib::SetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, kHomepageRlzString);

  string16 rlz;

  // Prime the cache.
  tracker_.set_assume_not_ui_thread(true);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kOmniboxRlzString, WideToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, &rlz));
  EXPECT_STREQ(kHomepageRlzString, WideToUTF8(rlz).c_str());

  // Make sure cache is valid.
  tracker_.set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kOmniboxRlzString, WideToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, &rlz));
  EXPECT_STREQ(kHomepageRlzString, WideToUTF8(rlz).c_str());

  // Perform ping.
  tracker_.set_assume_not_ui_thread(true);
  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();
  ExpectRlzPingSent(true);

  // Make sure cache is now updated.
  tracker_.set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz));
  EXPECT_STREQ(kNewOmniboxRlzString, WideToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, &rlz));
  EXPECT_STREQ(kNewHomepageRlzString, WideToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, ObserveHandlesBadArgs) {
  NavigationEntry entry(NULL, 0, GURL(), GURL(), string16(),
                        content::PAGE_TRANSITION_LINK, false);
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(NULL));
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(&entry));
}

TEST_F(RlzLibTest, ReactivationNonOrganicNonOrganic) {
  SetReactivationBrand("REAC");

  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationOrganicNonOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("REAC");

  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationNonOrganicOrganic) {
  SetMainBrand("TEST");
  SetReactivationBrand("GGLS");

  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(false);
}

TEST_F(RlzLibTest, ReactivationOrganicOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("GGRS");

  RLZTracker::InitRlzDelayed(true, 20, true, true);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(false);
}

