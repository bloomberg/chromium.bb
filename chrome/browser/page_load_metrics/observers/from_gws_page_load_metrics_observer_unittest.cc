// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {

const char kHistogramNameFromGWSFirstTextPaint[] =
    "PageLoad.Clients.FromGWS.Timing2.NavigationToFirstTextPaint";

}  // namespace

class TestFromGWSPageLoadMetricsObserver
    : public FromGWSPageLoadMetricsObserver {
 public:
  explicit TestFromGWSPageLoadMetricsObserver(const content::Referrer& referrer)
      : FromGWSPageLoadMetricsObserver(), referrer_(referrer) {}
  void OnCommit(content::NavigationHandle* navigation_handle) override {
    const GURL& url = navigation_handle->GetURL();
    SetCommittedURLAndReferrer(
        url, content::Referrer::SanitizeForRequest(url, referrer_));
  }

 private:
  const content::Referrer referrer_;

  DISALLOW_COPY_AND_ASSIGN(TestFromGWSPageLoadMetricsObserver);
};

class FromGWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new TestFromGWSPageLoadMetricsObserver(referrer_)));
  }

  // Inject this referrer to FromGWS observers.
  void set_referrer(const content::Referrer& referrer) { referrer_ = referrer; }

 protected:
  content::Referrer referrer_;
};

TEST_F(FromGWSPageLoadMetricsObserverTest, NoMetrics) {
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NoReferral) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("http://www.example.com"));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.google.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralsFromGWSHTTPToHTTPS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  // HTTPS google.com referral  to HTTP example.com.
  set_referrer(content::Referrer(GURL("https://www.google.com"),
                                 blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("http://www.example.com"));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                       timing.first_text_paint.InMilliseconds(),
                                       1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralFromGWS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);

  set_referrer(content::Referrer(GURL("https://www.google.com/url"),
                                 blink::WebReferrerPolicyDefault));
  NavigateAndCommit(GURL("https://www.example.com"));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                       timing.first_text_paint.InMilliseconds(),
                                       1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralFromGWSBackgroundLater) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);

  set_referrer(content::Referrer(GURL("https://www.google.com/url"),
                                 blink::WebReferrerPolicyDefault));
  NavigateAndCommit(GURL("https://www.example.com"));

  SimulateTimingUpdate(timing);
  web_contents()->WasHidden();

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                       timing.first_text_paint.InMilliseconds(),
                                       1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralsFromCaseInsensitive) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  // HTTPS google.com referral  to HTTP example.com.
  set_referrer(content::Referrer(GURL("https://wWw.GoOGlE.cOm/webhp"),
                                 blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                       timing.first_text_paint.InMilliseconds(),
                                       1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralsFromGWSOrigin) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  // HTTPS google.com referral  to HTTP example.com.
  set_referrer(content::Referrer(GURL("https://www.google.com"),
                                 blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example.com"));

  SimulateTimingUpdate(timing);

  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(10);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  PopulateRequiredTimingFields(&timing2);
  // HTTPS google.com referral  to HTTP example.com.
  set_referrer(content::Referrer(GURL("https://www.google.co.in"),
                                 blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example2.com"));

  SimulateTimingUpdate(timing2);
  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example3.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 2);
  histogram_tester().ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                       timing.first_text_paint.InMilliseconds(),
                                       1);
  histogram_tester().ExpectBucketCount(
      kHistogramNameFromGWSFirstTextPaint,
      timing2.first_text_paint.InMilliseconds(), 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ReferralNotFromGWS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  set_referrer(content::Referrer(GURL("https://www.anothersite.com"),
                                 blink::WebReferrerPolicyDefault));
  NavigateAndCommit(GURL("https://www.example.com"));

  SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.google.com"));
  histogram_tester().ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}
