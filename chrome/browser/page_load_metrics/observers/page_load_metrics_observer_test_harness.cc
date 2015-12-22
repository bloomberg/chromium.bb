// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

#include "base/memory/scoped_ptr.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"

namespace page_load_metrics {

namespace {

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  explicit TestPageLoadMetricsEmbedderInterface(
      PageLoadMetricsObserverTestHarness* test)
      : test_(test) {}

  rappor::RapporService* GetRapporService() override { return nullptr; }

  bool IsPrerendering(content::WebContents* web_contents) override {
    return false;
  }

  // Forward the registration logic to the test class so that derived classes
  // can override the logic there without depending on the embedder interface.
  void RegisterObservers(PageLoadTracker* tracker) override {
    test_->RegisterObservers(tracker);
  }

 private:
  PageLoadMetricsObserverTestHarness* test_;

  DISALLOW_COPY_AND_ASSIGN(TestPageLoadMetricsEmbedderInterface);
};

}  // namespace

PageLoadMetricsObserverTestHarness::PageLoadMetricsObserverTestHarness()
    : ChromeRenderViewHostTestHarness() {}

PageLoadMetricsObserverTestHarness::~PageLoadMetricsObserverTestHarness() {}

void PageLoadMetricsObserverTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("http://www.google.com"));
  observer_ =
      MetricsWebContentsObserver::CreateForWebContents(
          web_contents(),
          make_scoped_ptr(new TestPageLoadMetricsEmbedderInterface(this)));
  web_contents()->WasShown();
}

void PageLoadMetricsObserverTestHarness::StartNavigation(const GURL& gurl) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->StartNavigation(gurl);
}

void PageLoadMetricsObserverTestHarness::SimulateTimingUpdate(
    const PageLoadTiming& timing) {
  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());
}

const base::HistogramTester&
PageLoadMetricsObserverTestHarness::histogram_tester() const {
  return histogram_tester_;
}

}  // namespace page_load_metrics
