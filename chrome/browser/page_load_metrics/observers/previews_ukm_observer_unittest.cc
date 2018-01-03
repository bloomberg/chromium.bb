// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class TestPreviewsUKMObserver : public PreviewsUKMObserver {
 public:
  TestPreviewsUKMObserver(content::WebContents* web_contents,
                          bool data_reduction_proxy_used,
                          bool lite_page_received,
                          bool noscript_on)
      : web_contents_(web_contents),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        lite_page_received_(lite_page_received),
        noscript_on_(noscript_on) {}

  ~TestPreviewsUKMObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    ChromeNavigationData chrome_navigation_data;

    auto data_reduction_proxy_data =
        std::make_unique<data_reduction_proxy::DataReductionProxyData>();
    data_reduction_proxy_data->set_used_data_reduction_proxy(
        data_reduction_proxy_used_);
    data_reduction_proxy_data->set_request_url(GURL(kDefaultTestUrl));
    data_reduction_proxy_data->set_lite_page_received(lite_page_received_);
    chrome_navigation_data.SetDataReductionProxyData(
        std::move(data_reduction_proxy_data));

    if (noscript_on_)
      chrome_navigation_data.set_previews_state(content::NOSCRIPT_ON);

    content::WebContentsTester::For(web_contents_)
        ->SetNavigationData(navigation_handle,
                            chrome_navigation_data.ToValue());

    return PreviewsUKMObserver::OnCommit(navigation_handle, source_id);
  }

 private:
  content::WebContents* web_contents_;
  bool data_reduction_proxy_used_;
  bool lite_page_received_;
  bool noscript_on_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsUKMObserver);
};

class PreviewsUKMObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsUKMObserverTest() {}
  ~PreviewsUKMObserverTest() override {}

  void RunTest(bool data_reduction_proxy_used,
               bool lite_page_received,
               bool noscript_on) {
    data_reduction_proxy_used_ = data_reduction_proxy_used;
    lite_page_received_ = lite_page_received;
    noscript_on_ = noscript_on;
    NavigateAndCommit(GURL(kDefaultTestUrl));
  }

  void ValidateUKM(bool server_lofi_expected,
                   bool client_lofi_expected,
                   bool lite_page_expected,
                   bool noscript_expected,
                   bool opt_out_expected) {
    using UkmEntry = ukm::builders::Previews;
    auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (!server_lofi_expected && !client_lofi_expected && !lite_page_expected &&
        !noscript_expected && !opt_out_expected) {
      EXPECT_EQ(0u, entries.size());
      return;
    }
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GURL(kDefaultTestUrl));
      EXPECT_EQ(server_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kserver_lofiName));
      EXPECT_EQ(client_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kclient_lofiName));
      EXPECT_EQ(lite_page_expected, test_ukm_recorder().EntryHasMetric(
                                        entry, UkmEntry::klite_pageName));
      EXPECT_EQ(noscript_expected, test_ukm_recorder().EntryHasMetric(
                                       entry, UkmEntry::knoscriptName));
      EXPECT_EQ(opt_out_expected, test_ukm_recorder().EntryHasMetric(
                                      entry, UkmEntry::kopt_outName));
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<TestPreviewsUKMObserver>(
        web_contents(), data_reduction_proxy_used_, lite_page_received_,
        noscript_on_));
    // Data is only added to the first navigation after RunTest().
    data_reduction_proxy_used_ = false;
    lite_page_received_ = false;
    noscript_on_ = false;
  }

 private:
  bool data_reduction_proxy_used_ = false;
  bool lite_page_received_ = false;
  bool noscript_on_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUKMObserverTest);
};

TEST_F(PreviewsUKMObserverTest, NoPreviewSeen) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */, false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, UntrackedPreviewTypeOptOut) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);
  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();

  // Opt out should not be added sicne we don't track this type.
  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */, false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, LitePageSeen) {
  RunTest(true /* data_reduction_proxy_used */, true /* lite_page_received */,
          false /* noscript_on */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* noscript_expected */, false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, LitePageOptOut) {
  RunTest(true /* data_reduction_proxy_used */, true /* lite_page_received */,
          false /* noscript_on */);

  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* noscript_expected */, true /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptSeen) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          true /* noscript_on */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* noscript_expected */, false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptOptOut) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          true /* noscript_on */);

  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* noscript_expected */, true /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiSeen) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiOptOut) {
  RunTest(false /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              true /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiSeen) {
  RunTest(true /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiOptOut) {
  RunTest(true /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              true /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiSeen) {
  RunTest(true /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* opt_out_expected */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiOptOut) {
  RunTest(true /* data_reduction_proxy_used */, false /* lite_page_received */,
          false /* noscript_on */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      base::MakeUnique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(
      PreviewsInfoBarDelegate::OptOutEventKey());
  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              true /* opt_out_expected */);
}

}  // namespace

}  // namespace previews
