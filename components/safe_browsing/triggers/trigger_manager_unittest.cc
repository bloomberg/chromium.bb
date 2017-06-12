// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"
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
    EXPECT_CALL(*threat_details, FinishCollection(_, _)).Times(1);
    return threat_details;
  }
};

class TriggerManagerTest : public ::testing::Test {
 public:
  TriggerManagerTest() : trigger_manager_(/*ui_manager=*/nullptr) {}
  ~TriggerManagerTest() override {}

  void SetUp() override {
    ThreatDetails::RegisterFactory(&mock_threat_details_factory_);
  }

  content::WebContents* CreateWebContents() {
    return web_contents_factory_.CreateWebContents(&browser_context_);
  }

  bool StartCollectingThreatDetails(content::WebContents* web_contents) {
    return trigger_manager_.StartCollectingThreatDetails(
        web_contents, security_interstitials::UnsafeResource(), nullptr,
        nullptr);
  }

  bool FinishCollectingThreatDetails(content::WebContents* web_contents) {
    return trigger_manager_.FinishCollectingThreatDetails(
        web_contents, base::TimeDelta(), false, 0);
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

  DISALLOW_COPY_AND_ASSIGN(TriggerManagerTest);
};

TEST_F(TriggerManagerTest, StartAndFinishCollectingThreatDetails) {
  // Basic workflow is to start and finish data collection with a single
  // WebContents.
  content::WebContents* web_contents1 = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(data_collectors_map().empty());

  // More complex scenarios can happen, where collection happens on two
  // WebContents at the same time, possibly starting and completing in different
  // order.
  content::WebContents* web_contents2 = CreateWebContents();
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents2));
  EXPECT_THAT(data_collectors_map(),
              UnorderedElementsAre(Key(web_contents1), Key(web_contents2)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents2));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(data_collectors_map().empty());

  // Calling Start twice with the same WebContents is an error, and will return
  // false the second time. But it can still be completed normally.
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_FALSE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(data_collectors_map().empty());

  // Calling Finish twice with the same WebContents is an error, and will return
  // false the second time. It's basically a no-op.
  EXPECT_TRUE(StartCollectingThreatDetails(web_contents1));
  EXPECT_THAT(data_collectors_map(), UnorderedElementsAre(Key(web_contents1)));
  EXPECT_TRUE(FinishCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(data_collectors_map().empty());
  EXPECT_FALSE(FinishCollectingThreatDetails(web_contents1));
  EXPECT_TRUE(data_collectors_map().empty());
}

}  // namespace safe_browsing
