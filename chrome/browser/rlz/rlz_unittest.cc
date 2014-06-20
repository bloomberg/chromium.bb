// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/rlz.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "rlz/test/rlz_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

using content::NavigationEntry;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::AssertionFailure;

#if defined(OS_WIN)
using base::win::RegKey;
#endif

namespace {

// Dummy RLZ string for the access points.
const char kOmniboxRlzString[] = "test_omnibox";
const char kHomepageRlzString[] = "test_homepage";
const char kAppListRlzString[] = "test_applist";
const char kNewOmniboxRlzString[] = "new_omnibox";
const char kNewHomepageRlzString[] = "new_homepage";
const char kNewAppListRlzString[] = "new_applist";

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
  using RLZTracker::InitRlzDelayed;
  using RLZTracker::DelayedInit;
  using RLZTracker::Observe;

  TestRLZTracker() : assume_not_ui_thread_(true) {
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
  virtual void ScheduleDelayedInit(base::TimeDelta delay) OVERRIDE {
    // If the delay is 0, invoke the delayed init now. Otherwise,
    // don't schedule anything, it will be manually called during tests.
    if (delay == base::TimeDelta())
      DelayedInit();
  }

  virtual void ScheduleFinancialPing() OVERRIDE {
    PingNowImpl();
  }

  virtual bool ScheduleRecordProductEvent(rlz_lib::Product product,
                                          rlz_lib::AccessPoint point,
                                          rlz_lib::Event event_id) OVERRIDE {
    return !assume_not_ui_thread_;
  }

  virtual bool ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) OVERRIDE {
    return !assume_not_ui_thread_;
  }

  virtual bool ScheduleRecordFirstSearch(rlz_lib::AccessPoint point) OVERRIDE {
    return !assume_not_ui_thread_;
  }

#if defined(OS_CHROMEOS)
  virtual bool ScheduleClearRlzState() OVERRIDE {
    return !assume_not_ui_thread_;
  }
#endif

  virtual bool SendFinancialPing(const std::string& brand,
                                 const base::string16& lang,
                                 const base::string16& referral) OVERRIDE {
    // Don't ping the server during tests, just pretend as if we did.
    EXPECT_FALSE(brand.empty());
    pinged_brands_.insert(brand);

    // Set new access points RLZ string, like the actual server ping would have
    // done.
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(),
                               kNewOmniboxRlzString);
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(),
                               kNewHomepageRlzString);
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(),
                               kNewAppListRlzString);
    return true;
  }

  std::set<std::string> pinged_brands_;
  bool assume_not_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestRLZTracker);
};

class RlzLibTest : public RlzLibTestNoMachineState {
 protected:
  virtual void SetUp() OVERRIDE;

  void SetMainBrand(const char* brand);
  void SetReactivationBrand(const char* brand);
#if defined(OS_WIN)
  void SetRegistryBrandValue(const wchar_t* name, const char* brand);
#endif

  void SimulateOmniboxUsage();
  void SimulateHomepageUsage();
  void SimulateAppListUsage();
  void InvokeDelayedInit();

  void ExpectEventRecorded(const char* event_name, bool expected);
  void ExpectRlzPingSent(bool expected);
  void ExpectReactivationRlzPingSent(bool expected);

  TestRLZTracker tracker_;
#if defined(OS_POSIX)
  scoped_ptr<google_brand::BrandForTesting> brand_override_;
#endif
};

void RlzLibTest::SetUp() {
  RlzLibTestNoMachineState::SetUp();

  // Make sure a non-organic brand code is set in the registry or the RLZTracker
  // is pretty much a no-op.
  SetMainBrand("TEST");
  SetReactivationBrand("");
}

void RlzLibTest::SetMainBrand(const char* brand) {
#if defined(OS_WIN)
  SetRegistryBrandValue(google_update::kRegRLZBrandField, brand);
#elif defined(OS_POSIX)
  brand_override_.reset(new google_brand::BrandForTesting(brand));
#endif
  std::string check_brand;
  google_brand::GetBrand(&check_brand);
  EXPECT_EQ(brand, check_brand);
}

void RlzLibTest::SetReactivationBrand(const char* brand) {
  // TODO(thakis): Reactivation doesn't exist on Mac yet.
#if defined(OS_WIN)
  SetRegistryBrandValue(google_update::kRegRLZReactivationBrandField, brand);
  std::string check_brand;
  google_brand::GetReactivationBrand(&check_brand);
  EXPECT_EQ(brand, check_brand);
#endif
}

#if defined(OS_WIN)
void RlzLibTest::SetRegistryBrandValue(const wchar_t* name,
                                       const char* brand) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_SET_VALUE);
  if (*brand == 0) {
    LONG result = key.DeleteValue(name);
    ASSERT_TRUE(ERROR_SUCCESS == result || ERROR_FILE_NOT_FOUND == result);
  } else {
    base::string16 brand16 = base::ASCIIToUTF16(brand);
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name, brand16.c_str()));
  }
}
#endif

void RlzLibTest::SimulateOmniboxUsage() {
  // Create a dummy OmniboxLog object. The 'is_popup_open' field needs to be
  // true to trigger record of the first search. All other fields are passed in
  // with empty or invalid values.
  AutocompleteResult empty_result;
  OmniboxLog dummy(base::string16(), false, metrics::OmniboxInputType::INVALID,
                   true, 0, false, -1,
                   metrics::OmniboxEventProto::INVALID_SPEC,
                   base::TimeDelta::FromSeconds(0), 0,
                   base::TimeDelta::FromSeconds(0),
                   AutocompleteResult());

  tracker_.Observe(chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                   content::NotificationService::AllSources(),
                   content::Details<OmniboxLog>(&dummy));
}

void RlzLibTest::SimulateHomepageUsage() {
  scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
  entry->SetPageID(0);
  entry->SetTransitionType(content::PAGE_TRANSITION_HOME_PAGE);
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(entry.get()));
}

void RlzLibTest::SimulateAppListUsage() {
  RLZTracker::RecordAppListSearch();
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
  google_brand::GetBrand(&brand);
  EXPECT_EQ(expected, tracker_.was_ping_sent_for_brand(brand.c_str()));
}

void RlzLibTest::ExpectReactivationRlzPingSent(bool expected) {
  std::string brand;
  google_brand::GetReactivationBrand(&brand);
  EXPECT_EQ(expected, tracker_.was_ping_sent_for_brand(brand.c_str()));
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
//  Z: the user performs a search using the app list
//
// The events A to D happen in chronological order, but the other events
// may happen at any point between A-B or C-D, in no particular order.
//
// The visible results of the scenarios on Win are:
//
//  C1I event is recorded
//  C2I event is recorded
//  C7I event is recorded
//  C1F event is recorded
//  C2F event is recorded
//  C7F event is recorded
//  C1S event is recorded
//  C2S event is recorded
//  C7S event is recorded
//  RLZ ping sent
//
//  On Mac, C5 / C6 / C8 are sent instead of C1 / C2 / C7.
//  On ChromeOS, CA / CB / CC are sent, respectively.
//
// Variations on the above scenarios:
//
//  - if the delay specified to InitRlzDelayed() is negative, then the RLZ
//    ping should be sent out at the time of event X and not wait for I
//
// Also want to test that pre-warming the RLZ string cache works correctly.

#if defined(OS_WIN)
const char kOmniboxInstall[] = "C1I";
const char kOmniboxSetToGoogle[] = "C1S";
const char kOmniboxFirstSearch[] = "C1F";

const char kHomepageInstall[] = "C2I";
const char kHomepageSetToGoogle[] = "C2S";
const char kHomepageFirstSeach[] = "C2F";

const char kAppListInstall[] = "C7I";
const char kAppListSetToGoogle[] = "C7S";
const char kAppListFirstSearch[] = "C7F";
#elif defined(OS_MACOSX)
const char kOmniboxInstall[] = "C5I";
const char kOmniboxSetToGoogle[] = "C5S";
const char kOmniboxFirstSearch[] = "C5F";

const char kHomepageInstall[] = "C6I";
const char kHomepageSetToGoogle[] = "C6S";
const char kHomepageFirstSeach[] = "C6F";

const char kAppListInstall[] = "C8I";
const char kAppListSetToGoogle[] = "C8S";
const char kAppListFirstSearch[] = "C8F";
#elif defined(OS_CHROMEOS)
const char kOmniboxInstall[] = "CAI";
const char kOmniboxSetToGoogle[] = "CAS";
const char kOmniboxFirstSearch[] = "CAF";

const char kHomepageInstall[] = "CBI";
const char kHomepageSetToGoogle[] = "CBS";
const char kHomepageFirstSeach[] = "CBF";

const char kAppListInstall[] = "CCI";
const char kAppListSetToGoogle[] = "CCS";
const char kAppListFirstSearch[] = "CCF";
#endif

const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(20);

TEST_F(RlzLibTest, RecordProductEvent) {
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);

  ExpectEventRecorded(kOmniboxFirstSearch, true);
}

TEST_F(RlzLibTest, QuickStopAfterStart) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, true);

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, DelayedInitOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, true);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyGoogleAsStartup) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, false, false, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRunNoRlzStrings) {
  TestRLZTracker::InitRlzDelayed(false, false, kDelay, true, true, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, true);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRunNoRlzStringsGoogleAsStartup) {
  TestRLZTracker::InitRlzDelayed(false, false, kDelay, false, false, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRun) {
  // Set some dummy RLZ strings to simulate that we already ran before and
  // performed a successful ping to the RLZ server.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(), kHomepageRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(), kAppListRlzString);

  TestRLZTracker::InitRlzDelayed(false, false, kDelay, true, true, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoGoogleDefaultSearchOrHomepageOrStartup) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, false, false, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, true);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, HomepageUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, AppListUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, true);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, UsageBeforeDelayedInit) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateOmniboxUsage();
  SimulateHomepageUsage();
  SimulateAppListUsage();
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, true);
  ExpectEventRecorded(kOmniboxFirstSearch, true);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, true);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, UsageAfterDelayedInit) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();
  SimulateOmniboxUsage();
  SimulateHomepageUsage();
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, true);
  ExpectEventRecorded(kOmniboxFirstSearch, true);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, true);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageSendsPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, true, false);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, true);
  ExpectEventRecorded(kOmniboxSetToGoogle, true);
  ExpectEventRecorded(kOmniboxFirstSearch, true);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, HomepageUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, true, false);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, StartupUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, false, true);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, AppListUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, false, false);
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(kOmniboxInstall, false);
  ExpectEventRecorded(kOmniboxSetToGoogle, false);
  ExpectEventRecorded(kOmniboxFirstSearch, false);

  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSeach, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, true);

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, GetAccessPointRlzOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  base::string16 rlz;

  tracker_.set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, GetAccessPointRlzNotOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  base::string16 rlz;

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_FALSE(
      RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
}

TEST_F(RlzLibTest, GetAccessPointRlzIsCached) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  base::string16 rlz;

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_FALSE(
      RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));

  tracker_.set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());

  tracker_.set_assume_not_ui_thread(false);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, PingUpdatesRlzCache) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(), kHomepageRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(), kAppListRlzString);

  base::string16 rlz;

  // Prime the cache.
  tracker_.set_assume_not_ui_thread(true);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kAppListRlzString, base::UTF16ToUTF8(rlz).c_str());

  // Make sure cache is valid.
  tracker_.set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kAppListRlzString, base::UTF16ToUTF8(rlz).c_str());

  // Perform ping.
  tracker_.set_assume_not_ui_thread(true);
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();
  ExpectRlzPingSent(true);

  // Make sure cache is now updated.
  tracker_.set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kNewOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kNewHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kNewAppListRlzString, base::UTF16ToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, ObserveHandlesBadArgs) {
  scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
  entry->SetPageID(0);
  entry->SetTransitionType(content::PAGE_TRANSITION_LINK);
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(NULL));
  tracker_.Observe(content::NOTIFICATION_NAV_ENTRY_PENDING,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(entry.get()));
}

// TODO(thakis): Reactivation doesn't exist on Mac yet.
#if defined(OS_WIN)
TEST_F(RlzLibTest, ReactivationNonOrganicNonOrganic) {
  SetReactivationBrand("REAC");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationOrganicNonOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("REAC");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationNonOrganicOrganic) {
  SetMainBrand("TEST");
  SetReactivationBrand("GGLS");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(false);
}

TEST_F(RlzLibTest, ReactivationOrganicOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("GGRS");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(false);
}
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
TEST_F(RlzLibTest, ClearRlzState) {
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);

  ExpectEventRecorded(kOmniboxFirstSearch, true);

  RLZTracker::ClearRlzState();

  ExpectEventRecorded(kOmniboxFirstSearch, false);
}
#endif  // defined(OS_CHROMEOS)
