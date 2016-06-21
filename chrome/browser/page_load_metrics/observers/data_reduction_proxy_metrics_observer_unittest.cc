// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/renderer_host/chrome_navigation_data.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace data_reduction_proxy {

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

// Pingback client responsible for recording the timing information it receives
// from a SendPingback call.
class TestPingbackClient
    : public data_reduction_proxy::DataReductionProxyPingbackClient {
 public:
  TestPingbackClient()
      : data_reduction_proxy::DataReductionProxyPingbackClient(nullptr),
        send_pingback_called_(false) {}
  ~TestPingbackClient() override {}

  void SendPingback(
      const data_reduction_proxy::DataReductionProxyData& data,
      const data_reduction_proxy::DataReductionProxyPageLoadTiming& timing)
      override {
    timing_.reset(
        new data_reduction_proxy::DataReductionProxyPageLoadTiming(timing));
    send_pingback_called_ = true;
  }

  data_reduction_proxy::DataReductionProxyPageLoadTiming* timing() const {
    return timing_.get();
  }

  bool send_pingback_called() const { return send_pingback_called_; }

  void Reset() {
    send_pingback_called_ = false;
    timing_.reset();
  }

 private:
  std::unique_ptr<data_reduction_proxy::DataReductionProxyPageLoadTiming>
      timing_;
  bool send_pingback_called_;

  DISALLOW_COPY_AND_ASSIGN(TestPingbackClient);
};

}  // namespace

// DataReductionProxyMetricsObserver responsible for modifying data about the
// navigation in OnCommit. It is also responsible for using a passed in
// DataReductionProxyPingbackClient instead of the default.
class TestDataReductionProxyMetricsObserver
    : public DataReductionProxyMetricsObserver {
 public:
  TestDataReductionProxyMetricsObserver(content::WebContents* web_contents,
                                        TestPingbackClient* pingback_client,
                                        bool data_reduction_proxy_used,
                                        bool lofi_used)
      : web_contents_(web_contents),
        pingback_client_(pingback_client),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        lofi_used_(lofi_used) {}

  ~TestDataReductionProxyMetricsObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override {
    DataReductionProxyData* data =
        DataForNavigationHandle(web_contents_, navigation_handle);
    data->set_used_data_reduction_proxy(data_reduction_proxy_used_);
    data->set_lofi_requested(lofi_used_);
    DataReductionProxyMetricsObserver::OnCommit(navigation_handle);
  }

  DataReductionProxyPingbackClient* GetPingbackClient() const override {
    return pingback_client_;
  }

 private:
  content::WebContents* web_contents_;
  TestPingbackClient* pingback_client_;
  bool data_reduction_proxy_used_;
  bool lofi_used_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyMetricsObserver);
};

class DataReductionProxyMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  DataReductionProxyMetricsObserverTest()
      : pingback_client_(new TestPingbackClient()),
        data_reduction_proxy_used_(false),
        is_using_lofi_(false) {}

  void ResetTest() {
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_start = base::TimeDelta::FromSeconds(3);
    timing_.first_contentful_paint = base::TimeDelta::FromSeconds(4);
    timing_.first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.first_text_paint = base::TimeDelta::FromSeconds(6);
    timing_.load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(bool data_reduction_proxy_used, bool is_using_lofi) {
    data_reduction_proxy_used_ = data_reduction_proxy_used;
    is_using_lofi_ = is_using_lofi;
    NavigateAndCommit(GURL(kDefaultTestUrl));
    SimulateTimingUpdate(timing_);
    pingback_client_->Reset();

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL(kDefaultTestUrl2));
  }

  void ValidateTimes() {
    EXPECT_TRUE(pingback_client_->send_pingback_called());
    EXPECT_EQ(timing_.navigation_start,
              pingback_client_->timing()->navigation_start);
    EXPECT_EQ(timing_.first_contentful_paint,
              pingback_client_->timing()->first_contentful_paint);
    EXPECT_EQ(timing_.response_start,
              pingback_client_->timing()->response_start);
    EXPECT_EQ(timing_.load_event_start,
              pingback_client_->timing()->load_event_start);
    EXPECT_EQ(timing_.first_image_paint,
              pingback_client_->timing()->first_image_paint);
  }

  void ValidateHistograms() {
    ValidateHistogramsForSuffix(
        internal::kHistogramDOMContentLoadedEventFiredSuffix,
        timing_.dom_content_loaded_event_start);
    ValidateHistogramsForSuffix(internal::kHistogramFirstLayoutSuffix,
                                timing_.first_layout);
    ValidateHistogramsForSuffix(internal::kHistogramLoadEventFiredSuffix,
                                timing_.load_event_start);
    ValidateHistogramsForSuffix(internal::kHistogramFirstContentfulPaintSuffix,
                                timing_.first_contentful_paint);
    ValidateHistogramsForSuffix(internal::kHistogramFirstImagePaintSuffix,
                                timing_.first_image_paint);
    ValidateHistogramsForSuffix(internal::kHistogramFirstPaintSuffix,
                                timing_.first_paint);
    ValidateHistogramsForSuffix(internal::kHistogramFirstTextPaintSuffix,
                                timing_.first_text_paint);
    ValidateHistogramsForSuffix(internal::kHistogramParseStartSuffix,
                                timing_.parse_start);
  }

  void ValidateHistogramsForSuffix(const std::string& histogram_suffix,
                                   const base::TimeDelta& event) {
    histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(histogram_suffix),
        data_reduction_proxy_used_ ? 1 : 0);
    histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramDataReductionProxyLoFiOnPrefix)
            .append(histogram_suffix),
        is_using_lofi_ ? 1 : 0);
    if (!data_reduction_proxy_used_)
      return;
    histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(histogram_suffix),
        static_cast<base::HistogramBase::Sample>(event.InMilliseconds()), 1);
    if (!is_using_lofi_)
      return;
    histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyLoFiOnPrefix)
            .append(histogram_suffix),
        event.InMilliseconds(), is_using_lofi_ ? 1 : 0);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new TestDataReductionProxyMetricsObserver(
            web_contents(), pingback_client_.get(), data_reduction_proxy_used_,
            is_using_lofi_)));
  }

  std::unique_ptr<TestPingbackClient> pingback_client_;
  page_load_metrics::PageLoadTiming timing_;

 private:
  bool data_reduction_proxy_used_;
  bool is_using_lofi_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserverTest);
};

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOff) {
  ResetTest();
  // Verify that when the data reduction proxy was not used, no UMA is reported.
  RunTest(false, false);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOn) {
  ResetTest();
  // Verify that when the data reduction proxy was used, but lofi was not used,
  // the correpsonding UMA is reported.
  RunTest(true, false);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, LofiEnabled) {
  ResetTest();
  // Verify that when the data reduction proxy was used and lofi was used, both
  // histograms are reported.
  RunTest(true, true);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, OnCompletePingback) {
  ResetTest();
  // Verify that when data reduction proxy was used the correct timing
  // information is sent to SendPingback.
  RunTest(true, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but first image paint is
  // zero, the correct timing information is sent to SendPingback.
  timing_.first_image_paint = base::TimeDelta::FromSeconds(0);
  RunTest(true, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but first contentful paint
  // is zero, SendPingback is not called.
  timing_.first_contentful_paint = base::TimeDelta::FromSeconds(0);
  RunTest(true, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but load event start is
  // zero, SendPingback is not called.
  timing_.load_event_start = base::TimeDelta::FromSeconds(0);
  RunTest(true, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was not used, SendPingback is not
  // called.
  RunTest(false, false);
  EXPECT_FALSE(pingback_client_->send_pingback_called());

  ResetTest();
  // Verify that when the holdback experiment is enabled, no pingback is sent.
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  RunTest(true, false);
  EXPECT_FALSE(pingback_client_->send_pingback_called());
}

}  //  namespace data_reduction_proxy
