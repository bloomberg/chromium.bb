// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {
const char kExampleUrl[] = "http://www.example.com/";
}  // namespace

class FromGWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new FromGWSPageLoadMetricsObserver()));
  }
};

class FromGWSPageLoadMetricsLoggerTest : public testing::Test {};

TEST_F(FromGWSPageLoadMetricsObserverTest, NoMetrics) {
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NoPreviousCommittedUrl) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NonSearchPreviousCommittedUrl) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("http://www.other.com"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       GoogleNonSearchPreviousCommittedUrl1) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       GoogleNonSearchPreviousCommittedUrl2) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  // Navigation from /search, but missing a query string, so can't have been a
  // search results page.
  NavigateAndCommit(GURL("https://www.google.com/search?a=b&c=d"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl1) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp?q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl2) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl3) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl4) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchToNonSearchToOtherPage) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(2);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("http://www.example.com/other"));
  SimulateTimingUpdate(timing2);

  // Navigate again to force logging. We expect to log timing for the page
  // navigated from search, but not for the page navigated from that page.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchToNonSearchToSearch) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(2);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  SimulateTimingUpdate(timing2);

  // Navigate again to force logging. We expect to log timing for the page
  // navigated from search, but not for the search page we navigated to.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchToNonSearchToSearchToNonSearch) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(2);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  page_load_metrics::PageLoadTiming timing3;
  timing3.navigation_start = base::Time::FromDoubleT(3);
  timing3.first_text_paint = base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  PopulateRequiredTimingFields(&timing3);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  SimulateTimingUpdate(timing2);
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing3);

  // Navigate again to force logging. We expect to log timing for both pages
  // navigated from search, but not for the search pages we navigated to.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      2);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing3.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchToNonSearchToSearchToNonSearchBackgrounded) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(2);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  page_load_metrics::PageLoadTiming timing3;
  timing3.navigation_start = base::Time::FromDoubleT(3);
  timing3.first_text_paint = base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  PopulateRequiredTimingFields(&timing3);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  web_contents()->WasHidden();
  SimulateTimingUpdate(timing2);
  NavigateAndCommit(GURL(kExampleUrl));
  SimulateTimingUpdate(timing3);

  // Navigate again to force logging. We expect to log timing for the first page
  // navigated from search, but not the second since it was backgrounded.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchRedirectorPreviousCommittedUrl) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/search#q=test"));
  NavigateAndCommit(GURL("https://www.google.com/url?source=web"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       NonSearchRedirectorPreviousCommittedUrl) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp?q=test"));
  NavigateAndCommit(GURL("https://www.google.com/url?a=b&c=d"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchPreviousCommittedUrlBackgroundLater) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://www.google.com/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  SimulateTimingUpdate(timing);
  web_contents()->WasHidden();

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.final.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSFirstTextPaint,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, IsGoogleSearchHostname) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "www.google.com"},
      {true, "www.google.co.uk"},
      {true, "www.google.co.in"},
      {false, "other.google.com"},
      {false, "other.www.google.com"},
      {false, "www.other.google.com"},
      {false, "www.www.google.com"},
      {false, "www.google.appspot.com"},
      {false, "www.google.example.com"},
      // Search results are not served from the bare google.com domain.
      {false, "google.com"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              FromGWSPageLoadMetricsLogger::IsGoogleSearchHostname(test.url))
        << "for URL: " << test.url;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, IsGoogleSearchResultUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/#q=test"},
      {true, "https://www.google.com/search#q=test"},
      {true, "https://www.google.com/search?q=test"},
      {true, "https://www.google.com/webhp#q=test"},
      {true, "https://www.google.com/webhp?q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp#a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp?#a=b&q=test&c=d"},
      {false, "https://www.google.com/"},
      {false, "https://www.google.com/about/"},
      {false, "https://other.google.com/"},
      {false, "https://other.google.com/webhp?q=test"},
      {false, kExampleUrl},
      {false, "https://www.example.com/webhp?q=test"},
      {false, "https://google.com/#q=test"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(
        test.expected_result,
        FromGWSPageLoadMetricsLogger::IsGoogleSearchResultUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, IsGoogleRedirectorUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/url?"},
      {true, "https://www.google.com/url?a=b"},
      {true, "https://www.google.com/url?source=web"},
      {true, "https://www.google.com/url?a=b&source=web&c=d"},
      {true, "https://www.google.co.uk/url?source=web"},
      {false, "https://www.google.com/?"},
      {false, "https://www.google.com/?url"},
      {false, "https://www.example.com/url?source=web"},
      {false, "https://google.com/url?"},
      {false, "https://google.com/url?"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(
        test.expected_result,
        FromGWSPageLoadMetricsLogger::IsGoogleRedirectorUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, IsGoogleSearchRedirectorUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/url?source=web"},
      {true, "https://www.google.com/url?source=web#foo"},
      {false, "https://www.google.com/?source=web"},
      {false, "https://www.google.com/source=web"},
      {false, "https://www.example.com/url?source=web"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              FromGWSPageLoadMetricsLogger::IsGoogleSearchRedirectorUrl(
                  GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, QueryContainsComponent) {
  struct {
    bool expected_result;
    const char* query;
    const char* component;
  } test_cases[] = {
      {true, "a=b", "a=b"},
      {true, "a=b&c=d", "a=b"},
      {true, "a=b&c=d", "c=d"},
      {true, "a=b&c=d&e=f", "c=d"},
      {true, "za=b&a=b", "a=b"},
      {true, "a=bz&a=b", "a=b"},
      {true, "a=ba=b&a=b", "a=b"},
      {true, "a=a=a&a=a", "a=a"},
      {true, "source=web", "source=web"},
      {true, "a=b&source=web", "source=web"},
      {true, "a=b&source=web&c=d", "source=web"},
      {false, "a=a=a", "a=a"},
      {false, "", ""},
      {false, "a=b", ""},
      {false, "", "a=b"},
      {false, "za=b", "a=b"},
      {false, "za=bz", "a=b"},
      {false, "a=bz", "a=b"},
      {false, "za=b&c=d", "a=b"},
      {false, "a=b&c=dz", "c=d"},
      {false, "a=b&zc=d&e=f", "c=d"},
      {false, "a=b&c=dz&e=f", "c=d"},
      {false, "a=b&zc=dz&e=f", "c=d"},
      {false, "a=b&foosource=web&c=d", "source=web"},
      {false, "a=b&source=webbar&c=d", "source=web"},
      {false, "a=b&foosource=webbar&c=d", "source=web"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              FromGWSPageLoadMetricsLogger::QueryContainsComponent(
                  test.query, test.component))
        << "For query: " << test.query << " with component: " << test.component;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, QueryContainsComponentPrefix) {
  struct {
    bool expected_result;
    const char* query;
    const char* component;
  } test_cases[] = {
      {true, "a=b", "a="},
      {true, "a=b&c=d", "a="},
      {true, "a=b&c=d", "c="},
      {true, "a=b&c=d&e=f", "c="},
      {true, "za=b&a=b", "a="},
      {true, "ba=a=b&a=b", "a="},
      {true, "q=test", "q="},
      {true, "a=b&q=test", "q="},
      {true, "q=test&c=d", "q="},
      {true, "a=b&q=test&c=d", "q="},
      {false, "", ""},
      {false, "za=b", "a="},
      {false, "za=b&c=d", "a="},
      {false, "a=b&zc=d", "c="},
      {false, "a=b&zc=d&e=f", "c="},
      {false, "a=b&zq=test&c=d", "q="},
      {false, "ba=a=b", "a="},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              FromGWSPageLoadMetricsLogger::QueryContainsComponentPrefix(
                  test.query, test.component))
        << "For query: " << test.query << " with component: " << test.component;
  }
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, Basic) {
  FromGWSPageLoadMetricsLogger logger;
  ASSERT_FALSE(logger.ShouldLogMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NoPreviousPage) {
  FromGWSPageLoadMetricsLogger logger;
  logger.set_previously_committed_url(GURL());
  logger.set_navigation_initiated_via_link(true);
  ASSERT_FALSE(logger.ShouldLogMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationNotInitiatedViaLink) {
  FromGWSPageLoadMetricsLogger logger;
  logger.set_previously_committed_url(
      GURL("https://www.google.com/search?q=test"));
  logger.set_navigation_initiated_via_link(false);
  ASSERT_FALSE(logger.ShouldLogMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationFromSearchToSearch) {
  FromGWSPageLoadMetricsLogger logger;
  logger.set_previously_committed_url(
      GURL("https://www.google.com/search?q=test"));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_FALSE(
      logger.ShouldLogMetrics(GURL("https://www.google.com/search?q=test")));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationFromSearch) {
  FromGWSPageLoadMetricsLogger logger;
  logger.set_previously_committed_url(
      GURL("https://www.google.com/search?q=test"));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_TRUE(logger.ShouldLogMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationFromSearchRedirector) {
  FromGWSPageLoadMetricsLogger logger;
  logger.set_previously_committed_url(
      GURL("https://www.google.com/url?source=web"));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_TRUE(logger.ShouldLogMetrics(GURL(kExampleUrl)));
}
