// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"

namespace page_load_metrics {

namespace {

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  explicit TestPageLoadMetricsEmbedderInterface(
      PageLoadMetricsObserverTestHarness* test)
      : test_(test) {}

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

// static
void PageLoadMetricsObserverTestHarness::PopulateRequiredTimingFields(
    PageLoadTiming* inout_timing) {
  if (!inout_timing->first_contentful_paint.is_zero() &&
      inout_timing->first_paint.is_zero()) {
    inout_timing->first_paint = inout_timing->first_contentful_paint;
  }
  if (!inout_timing->first_text_paint.is_zero() &&
      inout_timing->first_paint.is_zero()) {
    inout_timing->first_paint = inout_timing->first_text_paint;
  }
  if (!inout_timing->first_image_paint.is_zero() &&
      inout_timing->first_paint.is_zero()) {
    inout_timing->first_paint = inout_timing->first_image_paint;
  }
  if (!inout_timing->first_paint.is_zero() &&
      inout_timing->first_layout.is_zero()) {
    inout_timing->first_layout = inout_timing->first_paint;
  }
  if (!inout_timing->load_event_start.is_zero() &&
      inout_timing->dom_content_loaded_event_start.is_zero()) {
    inout_timing->dom_content_loaded_event_start =
        inout_timing->load_event_start;
  }
  if (!inout_timing->first_layout.is_zero() &&
      inout_timing->dom_loading.is_zero()) {
    inout_timing->dom_loading = inout_timing->first_layout;
  }
  if (!inout_timing->dom_content_loaded_event_start.is_zero() &&
      inout_timing->dom_loading.is_zero()) {
    inout_timing->dom_loading = inout_timing->dom_content_loaded_event_start;
  }
  if (!inout_timing->dom_loading.is_zero() &&
      inout_timing->response_start.is_zero()) {
    inout_timing->response_start = inout_timing->dom_loading;
  }
}

void PageLoadMetricsObserverTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("http://www.google.com"));
  observer_ = MetricsWebContentsObserver::CreateForWebContents(
      web_contents(),
      base::WrapUnique(new TestPageLoadMetricsEmbedderInterface(this)));
  web_contents()->WasShown();
}

void PageLoadMetricsObserverTestHarness::StartNavigation(const GURL& gurl) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->StartNavigation(gurl);
}

void PageLoadMetricsObserverTestHarness::SimulateTimingUpdate(
    const PageLoadTiming& timing) {
  SimulateTimingAndMetadataUpdate(timing, PageLoadMetadata());
}

void PageLoadMetricsObserverTestHarness::SimulateTimingAndMetadataUpdate(
    const PageLoadTiming& timing,
    const PageLoadMetadata& metadata) {
  observer_->OnMessageReceived(PageLoadMetricsMsg_TimingUpdated(
                                   observer_->routing_id(), timing, metadata),
                               web_contents()->GetMainFrame());
}

const base::HistogramTester&
PageLoadMetricsObserverTestHarness::histogram_tester() const {
  return histogram_tester_;
}

}  // namespace page_load_metrics
