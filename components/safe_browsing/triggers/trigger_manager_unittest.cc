// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"

#include "base/stl_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Key;
using testing::UnorderedElementsAre;

namespace safe_browsing {

// Mock ThreatDetails class that makes FinishCollection a no-op.
class MockThreatDetails : public ThreatDetails {
 public:
  MockThreatDetails() : ThreatDetails() {}
  MOCK_METHOD2(FinishCollection, void(bool did_proceed, int num_visits));

 private:
  ~MockThreatDetails() override {}
  DISALLOW_COPY_AND_ASSIGN(MockThreatDetails);
};

class MockThreatDetailsFactory : public ThreatDetailsFactory {
 public:
  ~MockThreatDetailsFactory() override {}

  ThreatDetails* CreateThreatDetails(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      net::URLRequestContextGetter* request_context_getter,
      history::HistoryService* history_service) override {
    MockThreatDetails* threat_details = new MockThreatDetails();
    return threat_details;
  }
};

class TriggerManagerTest : public ::testing::Test {
 public:
  TriggerManagerTest() : trigger_manager_(/*ui_manager=*/nullptr) {}
  ~TriggerManagerTest() override {}

  void SetUp() override {
    ThreatDetails::RegisterFactory(&mock_threat_details_factory_);

    // Register any prefs that are needed by the trigger manager. By default,
    // enable Safe Browsing and Extended Reporting.
    safe_browsing::RegisterProfilePrefs(pref_service_.registry());
    SetPref(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    SetPref(prefs::kSafeBrowsingScoutReportingEnabled, true);
    SetPref(prefs::kSafeBrowsingScoutGroupSelected, true);
  }

  void SetPref(const std::string& pref, bool value) {
    pref_service_.SetBoolean(pref, value);
  }

  content::WebContents* CreateWebContents() {
    DCHECK(!browser_context_.IsOffTheRecord())
        << "CreateWebContents() should not be called after "
           "CreateIncognitoWebContents()";
    return web_contents_factory_.CreateWebContents(&browser_context_);
  }

  content::WebContents* CreateIncognitoWebContents() {
    browser_context_.set_is_off_the_record(true);
    return web_contents_factory_.CreateWebContents(&browser_context_);
  }

  bool StartCollectingThreatDetails(content::WebContents* web_contents) {
    SBErrorOptions options =
        TriggerManager::GetSBErrorDisplayOptions(pref_service_, *web_contents);
    return trigger_manager_.StartCollectingThreatDetails(
        web_contents, security_interstitials::UnsafeResource(), nullptr,
        nullptr, options);
  }

  bool FinishCollectingThreatDetails(content::WebContents* web_contents,
                                     bool expect_report_sent) {
    if (expect_report_sent) {
      MockThreatDetails* threat_details = static_cast<MockThreatDetails*>(
          trigger_manager_.data_collectors_map_[web_contents].get());
      EXPECT_CALL(*threat_details, FinishCollection(_, _)).Times(1);
    }
    SBErrorOptions options =
        TriggerManager::GetSBErrorDisplayOptions(pref_service_, *web_contents);
    return trigger_manager_.FinishCollectingThreatDetails(
        web_contents, base::TimeDelta(), false, 0, options);
  }

  const DataCollectorsMap& data_collectors_map() {
    return trigger_manager_.data_collectors_map_;
  }

 private:
  TriggerManager trigger_manager_;
  MockThreatDetailsFactory mock_threat_details_factory_;
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory web_contents_factory_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(TriggerManagerTest);
};

TEST_F(TriggerManagerTest, StartAndFinishCollectingThreatDetails) {
  // Basic workflow is to start and finish data collection with a single
  // WebContents.
  content::WebContents* web_contents1 = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1, true));
  EXPECT_TRUE(data_collectors_map().empty());

  // More complex scenarios can happen, where collection happens on two
  // WebContents at the same time, possibly starting and completing in different
  // order.
  content::WebContents* web_contents2 = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents2));
  EXPECT_THAT(data_collectors_map(),
              UnorderedElementsAre(Key(web_contents1), Key(web_contents2)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents2, true));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1, true));
  EXPECT_TRUE(data_collectors_map().empty());

  // Calling Start twice with the same WebContents is an error, and will return
  // false the second time. But it can still be completed normally.
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_FALSE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1, true));
  EXPECT_TRUE(data_collectors_map().empty());

  // Calling Finish twice with the same WebContents is an error, and will return
  // false the second time. It's basically a no-op.
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1, true));
  EXPECT_TRUE(data_collectors_map().empty());
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents1, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, NoDataCollection_Incognito) {
  // Data collection will not begin and no reports will be sent when incognito.
  content::WebContents* web_contents = CreateIncognitoWebContents();
  EXPECT_FALSE(StartCollectingThreatDetails(web_contents));
  EXPECT_TRUE(data_collectors_map().empty());
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, NoDataCollection_SBEROptInDisallowed) {
  // Data collection will not begin and no reports will be sent when the user is
  // not allowed to opt-in to SBER.
  SetPref(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  content::WebContents* web_contents = CreateWebContents();
  EXPECT_FALSE(StartCollectingThreatDetails(web_contents));
  EXPECT_TRUE(data_collectors_map().empty());
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, NoDataCollection_IncognitoAndSBEROptInDisallowed) {
  // Data collection will not begin and no reports will be sent when the user is
  // not allowed to opt-in to SBER and is also incognito.
  SetPref(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  content::WebContents* web_contents = CreateIncognitoWebContents();
  EXPECT_FALSE(StartCollectingThreatDetails(web_contents));
  EXPECT_TRUE(data_collectors_map().empty());
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, UserOptedOutOfSBER_DataCollected_NoReportSent) {
  // When the user is opted-out of SBER then data collection will begin but no
  // report will be sent when data collection ends.
  SetPref(prefs::kSafeBrowsingScoutReportingEnabled, false);
  content::WebContents* web_contents = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents)));
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, UserOptsOutOfSBER_DataCollected_NoReportSent) {
  // If the user opts-out of Extended Reporting while data is being collected
  // then no report is sent. Note that the test fixture opts the user into
  // Extended Reporting by default.
  content::WebContents* web_contents = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents)));

  SetPref(prefs::kSafeBrowsingScoutReportingEnabled, false);

  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest, UserOptsInToSBER_DataCollected_ReportSent) {
  // When the user is opted-out of SBER then data collection will begin. If they
  // opt-in to SBER while data collection is in progress then the report will
  // also be sent.
  SetPref(prefs::kSafeBrowsingScoutReportingEnabled, false);
  content::WebContents* web_contents = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents)));

  SetPref(prefs::kSafeBrowsingScoutReportingEnabled, true);

  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents, true));
  EXPECT_TRUE(data_collectors_map().empty());
}

TEST_F(TriggerManagerTest,
       SBEROptInBecomesDisallowed_DataCollected_NoReportSent) {
  // If the user loses the ability to opt-in to SBER in the middle of data
  // collection then the report will not be sent.
  content::WebContents* web_contents = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents)));

  // Remove the ability to opt-in to SBER.
  SetPref(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);

  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents, false));
  EXPECT_TRUE(data_collectors_map().empty());
}
}  // namespace safe_browsing
