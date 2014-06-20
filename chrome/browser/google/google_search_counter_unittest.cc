// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_counter.h"
#include "components/google/core/browser/google_search_metrics.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSearchMetrics : public GoogleSearchMetrics {
 public:
  MOCK_CONST_METHOD1(RecordGoogleSearch,
      void(GoogleSearchMetrics::AccessPoint ap));
};

}  // namespace

class GoogleSearchCounterTest : public testing::Test {
 protected:
  GoogleSearchCounterTest();
  virtual ~GoogleSearchCounterTest();

  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  // Test if |url| is a Google search for specific types. When |is_omnibox| is
  // true, this method will append Omnibox identifiers to the simulated URL
  // navigation. If |expected_metric| is set and not AP_BOUNDARY, we'll also use
  // the Search Metrics mock class to ensure that the type of metric recorded is
  // correct. Note that when |expected_metric| is AP_BOUNDARY, we strictly
  // forbid any metrics from being logged at all. See implementation below for
  // details.
  void TestGoogleSearch(const std::string& url,
                        bool is_omnibox,
                        GoogleSearchMetrics::AccessPoint expected_metric);

 private:
  void ExpectMetricsLogged(GoogleSearchMetrics::AccessPoint ap);

  // Weak ptr. Actual instance owned by GoogleSearchCounter.
  ::testing::StrictMock<MockSearchMetrics>* mock_search_metrics_;
};

GoogleSearchCounterTest::GoogleSearchCounterTest()
    : mock_search_metrics_(NULL) {
}

GoogleSearchCounterTest::~GoogleSearchCounterTest() {
}

void GoogleSearchCounterTest::SetUp() {
  // Keep a weak ptr to MockSearchMetrics so we can run expectations. The
  // GoogleSearchCounter singleton will own and clean up MockSearchMetrics.
  mock_search_metrics_ = new ::testing::StrictMock<MockSearchMetrics>;
  GoogleSearchCounter::GetInstance()->SetSearchMetricsForTesting(
      mock_search_metrics_);
}

void GoogleSearchCounterTest::TearDown() {
  mock_search_metrics_ = NULL;
}

void GoogleSearchCounterTest::TestGoogleSearch(
    const std::string& url,
    bool is_omnibox,
    GoogleSearchMetrics::AccessPoint expected_metric) {
  content::LoadCommittedDetails details;
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  if (is_omnibox) {
    entry->SetTransitionType(content::PageTransitionFromInt(
        content::PAGE_TRANSITION_GENERATED |
            content::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  }
  entry->SetURL(GURL(url));
  details.entry = entry.get();

  // Since the internal mocked metrics object is strict, if |expect_metrics| is
  // false, the absence of this call to ExpectMetricsLogged will be noticed and
  // cause the test to complain, as expected. We use this behaviour to test
  // negative test cases (such as bad searches).
  if (expected_metric != GoogleSearchMetrics::AP_BOUNDARY)
    ExpectMetricsLogged(expected_metric);

  // For now we don't care about the notification source, but when we start
  // listening for additional access points, we will have to pass in a valid
  // controller.
  GoogleSearchCounter::GetInstance()->Observe(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(NULL),
      content::Details<content::LoadCommittedDetails>(&details));
}

void GoogleSearchCounterTest::ExpectMetricsLogged(
    GoogleSearchMetrics::AccessPoint ap) {
  EXPECT_CALL(*mock_search_metrics_, RecordGoogleSearch(ap)).Times(1);
}

TEST_F(GoogleSearchCounterTest, EmptySearch) {
  TestGoogleSearch(std::string(), false, GoogleSearchMetrics::AP_BOUNDARY);
}

TEST_F(GoogleSearchCounterTest, GoodOmniboxSearch) {
  TestGoogleSearch("http://www.google.com/search?q=something", true,
                   GoogleSearchMetrics::AP_OMNIBOX);
}

TEST_F(GoogleSearchCounterTest, BadOmniboxSearch) {
  TestGoogleSearch("http://www.google.com/search?other=something", true,
                   GoogleSearchMetrics::AP_BOUNDARY);
}

TEST_F(GoogleSearchCounterTest, EmptyOmniboxSearch) {
  TestGoogleSearch(std::string(), true, GoogleSearchMetrics::AP_BOUNDARY);
}

TEST_F(GoogleSearchCounterTest, GoodOtherSearch) {
  TestGoogleSearch("http://www.google.com/search?q=something", false,
                   GoogleSearchMetrics::AP_OTHER);
}

TEST_F(GoogleSearchCounterTest, BadOtherSearch) {
  TestGoogleSearch("http://www.google.com/search?other=something", false,
                   GoogleSearchMetrics::AP_BOUNDARY);
}

TEST_F(GoogleSearchCounterTest, SearchAppSearch) {
  TestGoogleSearch("http://www.google.com/webhp?source=search_app#q=something",
                   false, GoogleSearchMetrics::AP_SEARCH_APP);
}

TEST_F(GoogleSearchCounterTest, SearchAppStart) {
  // Starting the search app takes you to this URL, but it should not be
  // considered an actual search event. Note that this URL is not considered an
  // actual search because it has no query string parameter ("q").
  TestGoogleSearch("http://www.google.com/webhp?source=search_app",
                   false, GoogleSearchMetrics::AP_BOUNDARY);
}

// TODO(stevet): Add a regression test to protect against the particular
// bad-flags handling case that asvitkine pointed out.
