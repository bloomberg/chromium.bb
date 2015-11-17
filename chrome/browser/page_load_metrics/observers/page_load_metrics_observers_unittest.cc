// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {

const char kHistogramNameFromGWSFirstTextPaint[] =
    "PageLoad.Clients.FromGWS.Timing2.NavigationToFirstTextPaint";

}  // namespace

class TestPageLoadMetricsEmbedderInterface
    : public page_load_metrics::PageLoadMetricsEmbedderInterface {
 public:
  TestPageLoadMetricsEmbedderInterface() {}
  rappor::RapporService* GetRapporService() override { return nullptr; }
  bool IsPrerendering(content::WebContents* web_contents) override {
    return false;
  }
};

class TestFromGWSPageLoadMetricsObserver
    : public FromGWSPageLoadMetricsObserver {
 public:
  explicit TestFromGWSPageLoadMetricsObserver(
      page_load_metrics::PageLoadMetricsObservable* metrics)
      : FromGWSPageLoadMetricsObserver(metrics) {}
  void OnCommit(content::NavigationHandle* navigation_handle) override {
    const GURL& url = navigation_handle->GetURL();
    SetCommittedURLAndReferrer(
        url, content::Referrer::SanitizeForRequest(url, referrer_));
  }

  void set_referrer(const content::Referrer& referrer) { referrer_ = referrer; }

 private:
  content::Referrer referrer_;
};

class PageLoadMetricsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  PageLoadMetricsObserverTest() : ChromeRenderViewHostTestHarness() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("http://www.google.com"));
    observer_ =
        make_scoped_ptr(new page_load_metrics::MetricsWebContentsObserver(
            web_contents(),
            make_scoped_ptr(new TestPageLoadMetricsEmbedderInterface())));
    observer_->WasShown();

    // Add PageLoadMetricsObservers here.
    gws_observer_ = new TestFromGWSPageLoadMetricsObserver(observer_.get());
    observer_->AddObserver(gws_observer_);
  }

  base::HistogramTester histogram_tester_;
  scoped_ptr<page_load_metrics::MetricsWebContentsObserver> observer_;

  // PageLoadMetricsObservers:
  TestFromGWSPageLoadMetricsObserver* gws_observer_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsObserverTest);
};

TEST_F(PageLoadMetricsObserverTest, NoMetrics) {
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}

TEST_F(PageLoadMetricsObserverTest, NoReferral) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  NavigateAndCommit(GURL("http://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  // Navigate again to force logging.
  NavigateAndCommit(GURL("http://www.google.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}

TEST_F(PageLoadMetricsObserverTest, ReferralsFromGWSHTTPToHTTPS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  // HTTPS google.com referral  to HTTP example.com.
  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.google.com"), blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("http://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing.first_text_paint.InMilliseconds(),
                                      1);
}

TEST_F(PageLoadMetricsObserverTest, ReferralFromGWS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);

  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.google.com/url"), blink::WebReferrerPolicyDefault));
  NavigateAndCommit(GURL("https://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing.first_text_paint.InMilliseconds(),
                                      1);
}

TEST_F(PageLoadMetricsObserverTest, ReferralFromGWSBackgroundLater) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMicroseconds(1);

  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.google.com/url"), blink::WebReferrerPolicyDefault));
  NavigateAndCommit(GURL("https://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  observer_->WasHidden();

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing.first_text_paint.InMilliseconds(),
                                      1);
}

TEST_F(PageLoadMetricsObserverTest, ReferralsFromCaseInsensitive) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  // HTTPS google.com referral  to HTTP example.com.
  gws_observer_->set_referrer(content::Referrer(
      GURL("https://wWw.GoOGlE.cOm/webhp"), blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example2.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing.first_text_paint.InMilliseconds(),
                                      1);
}

TEST_F(PageLoadMetricsObserverTest, ReferralsFromGWSOrigin) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  // HTTPS google.com referral  to HTTP example.com.
  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.google.com"), blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(10);
  timing2.first_text_paint = base::TimeDelta::FromMilliseconds(100);
  // HTTPS google.com referral  to HTTP example.com.
  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.google.co.in"), blink::WebReferrerPolicyOrigin));
  NavigateAndCommit(GURL("https://www.example2.com"));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing2),
      web_contents()->GetMainFrame());
  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.example3.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 2);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing.first_text_paint.InMilliseconds(),
                                      1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFromGWSFirstTextPaint,
                                      timing2.first_text_paint.InMilliseconds(),
                                      1);
}

TEST_F(PageLoadMetricsObserverTest, ReferralNotFromGWS) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_text_paint = base::TimeDelta::FromMilliseconds(1);
  NavigateAndCommit(GURL("https://www.example.com"));
  gws_observer_->set_referrer(content::Referrer(
      GURL("https://www.anothersite.com"), blink::WebReferrerPolicyDefault));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  // Navigate again to force logging.
  NavigateAndCommit(GURL("https://www.google.com"));
  histogram_tester_.ExpectTotalCount(kHistogramNameFromGWSFirstTextPaint, 0);
}

TEST_F(PageLoadMetricsObserverTest, IsGoogleCaptcha) {
  struct {
    std::string url;
    bool expected;
  } test_cases[] = {
      {"", false},
      {"http://www.google.com/", false},
      {"http://www.cnn.com/", false},
      {"http://ipv4.google.com/", false},
      {"https://ipv4.google.com/sorry/IndexRedirect?continue=http://a", true},
      {"https://ipv6.google.com/sorry/IndexRedirect?continue=http://a", true},
      {"https://ipv7.google.com/sorry/IndexRedirect?continue=http://a", false},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected,
              google_captcha_observer::IsGoogleCaptcha(GURL(test.url)))
        << "for URL: " << test.url;
  }
}
