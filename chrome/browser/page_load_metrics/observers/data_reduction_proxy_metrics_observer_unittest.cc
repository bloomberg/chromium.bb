// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/renderer_host/chrome_navigation_data.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrl2[] = "https://example.com";

data_reduction_proxy::DataReductionProxyData* DataForNavigationHandle(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle) {
  ChromeNavigationData* chrome_navigation_data = new ChromeNavigationData();
  content::WebContentsTester::For(web_contents)
      ->SetNavigationData(navigation_handle,
                          base::WrapUnique(chrome_navigation_data));
  data_reduction_proxy::DataReductionProxyData* data =
      new data_reduction_proxy::DataReductionProxyData();
  chrome_navigation_data->SetDataReductionProxyData(base::WrapUnique(data));

  return data;
}

}  // namespace

class DataReductionProxyMetricsObserverWrapper
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataReductionProxyMetricsObserverWrapper(
      data_reduction_proxy::DataReductionProxyMetricsObserver* observer,
      content::WebContents* web_contents,
      bool data_reduction_proxy_used,
      bool lofi_used)
      : observer_(observer),
        web_contents_(web_contents),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        lofi_used_(lofi_used) {}

  ~DataReductionProxyMetricsObserverWrapper() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override {
    data_reduction_proxy::DataReductionProxyData* data =
        DataForNavigationHandle(web_contents_, navigation_handle);
    data->set_used_data_reduction_proxy(data_reduction_proxy_used_);
    data->set_lofi_requested(lofi_used_);
    observer_->OnCommit(navigation_handle);
  }

  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override {
    observer_->OnFirstContentfulPaint(timing, info);
  }

 private:
  std::unique_ptr<data_reduction_proxy::DataReductionProxyMetricsObserver>
      observer_;
  content::WebContents* web_contents_;
  bool data_reduction_proxy_used_;
  bool lofi_used_;
};

class DataReductionProxyMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  DataReductionProxyMetricsObserverTest()
      : data_reduction_proxy_used_(false), is_using_lofi_(false) {}

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new DataReductionProxyMetricsObserverWrapper(
            new data_reduction_proxy::DataReductionProxyMetricsObserver(),
            web_contents(), data_reduction_proxy_used_, is_using_lofi_)));
  }

  bool data_reduction_proxy_used_;
  bool is_using_lofi_;
};

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOff) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force UMA. UMA is not recorded until OnComplete, which
  // happens when a new navigation occurs.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxy,
      0);
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxyLoFiOn,
      0);
}

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOn) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(1);
  PopulateRequiredTimingFields(&timing);

  data_reduction_proxy_used_ = true;
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force UMA. UMA is not recorded until OnComplete, which
  // happens when a new navigation occurs.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxy,
      1);
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxyLoFiOn,
      0);
}

TEST_F(DataReductionProxyMetricsObserverTest, LofiEnabled) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(1);
  PopulateRequiredTimingFields(&timing);

  data_reduction_proxy_used_ = true;
  is_using_lofi_ = true;
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force UMA. UMA is not recorded until OnComplete, which
  // happens when a new navigation occurs.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxy,
      1);
  histogram_tester().ExpectTotalCount(
      data_reduction_proxy::internal::
          kHistogramFirstContentfulPaintDataReductionProxyLoFiOn,
      1);
}
