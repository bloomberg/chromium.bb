// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_counter.h"
#include "chrome/browser/google/google_search_metrics.h"
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

  void TestOmniboxSearch(const std::string& url, bool expect_metrics);

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

void GoogleSearchCounterTest::TestOmniboxSearch(const std::string& url,
                                                bool expect_metrics) {
  content::LoadCommittedDetails details;
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  entry->SetTransitionType(content::PAGE_TRANSITION_GENERATED);
  entry->SetURL(GURL(url));
  details.entry = entry.get();

  // Since the internal mocked metrics object is strict, if |expect_metrics| is
  // false, the absence of this call to ExpectMetricsLogged will be noticed and
  // cause the test to complain, as expected. We use this behaviour to test
  // negative test cases (such as bad searches).
  if (expect_metrics)
    ExpectMetricsLogged(GoogleSearchMetrics::AP_OMNIBOX);

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

TEST_F(GoogleSearchCounterTest, GoodOmniboxSearch) {
  TestOmniboxSearch("http://www.google.com/search?q=something", true);
}

TEST_F(GoogleSearchCounterTest, BadOmniboxSearch) {
  TestOmniboxSearch("http://www.google.com/search?other=something", false);
}

TEST_F(GoogleSearchCounterTest, EmptyOmniboxSearch) {
  TestOmniboxSearch(std::string(), false);
}

// TODO(stevet): Add a regression test to protect against the participar
// bad-flags handling case that asvitkine pointed out.
