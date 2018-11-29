// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_lite_page_redirect_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_test_utils.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_pingback_client_impl.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/previews/content/previews_user_data.h"

class TestPreviewsLitePageRedirectMetricsObserver
    : public PreviewsLitePageRedirectMetricsObserver {
 public:
  TestPreviewsLitePageRedirectMetricsObserver(
      content::WebContents* web_contents,
      data_reduction_proxy::TestPingbackClient* pingback_client,
      previews::PreviewsUserData::ServerLitePageInfo* preview_info,
      net::EffectiveConnectionType ect)
      : web_contents_(web_contents),
        pingback_client_(pingback_client),
        preview_info_(preview_info),
        ect_(ect) {}

  ~TestPreviewsLitePageRedirectMetricsObserver() override {}

  // PreviewsLitePageRedirectMetricsObserver:
  ObservePolicy OnCommitCalled(content::NavigationHandle* navigation_handle,
                               ukm::SourceId source_id) override {
    previews::PreviewsUserData* data =
        PreviewsUITabHelper::FromWebContents(web_contents_)
            ->CreatePreviewsUserDataForNavigationHandle(navigation_handle, 0u);

    if (preview_info_) {
      data->set_server_lite_page_info(preview_info_->Clone());
      data->set_navigation_ect(ect_);
    }

    return PreviewsLitePageRedirectMetricsObserver::OnCommitCalled(
        navigation_handle, source_id);
  }

  data_reduction_proxy::DataReductionProxyPingbackClient* GetPingbackClient()
      const override {
    return pingback_client_;
  }

  void RequestProcessDump(
      base::ProcessId pid,
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback) override {
    memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump(
        memory_instrumentation::mojom::GlobalMemoryDump::New());

    memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
        memory_instrumentation::mojom::ProcessMemoryDump::New());
    pmd->pid = pid;
    pmd->process_type = memory_instrumentation::mojom::ProcessType::RENDERER;
    pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    pmd->os_dump->private_footprint_kb = data_reduction_proxy::kMemoryKb;

    global_dump->process_dumps.push_back(std::move(pmd));
    std::move(callback).Run(true,
                            memory_instrumentation::GlobalMemoryDump::MoveFrom(
                                std::move(global_dump)));
  }

 private:
  content::WebContents* web_contents_;
  data_reduction_proxy::TestPingbackClient* pingback_client_;
  previews::PreviewsUserData::ServerLitePageInfo* preview_info_;
  net::EffectiveConnectionType ect_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsLitePageRedirectMetricsObserver);
};

class PreviewsLitePageRedirectMetricsObserverTest
    : public data_reduction_proxy::DataReductionProxyMetricsObserverTestBase {
 public:
  PreviewsLitePageRedirectMetricsObserverTest() {}
  ~PreviewsLitePageRedirectMetricsObserverTest() override {}

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestPreviewsLitePageRedirectMetricsObserver>(
            web_contents(), pingback_client(), preview_info(), ect()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectMetricsObserverTest);
};

TEST_F(PreviewsLitePageRedirectMetricsObserverTest, TestPingbackSent) {
  previews::PreviewsUserData::ServerLitePageInfo info;
  info.drp_session_key = "meow";
  info.page_id = 1337U;

  ResetTest();
  RunLitePageRedirectTest(&info, net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  NavigateToUntrackedUrl();
  ValidatePreviewsStateInPingback();
  ValidateTimes();
}

TEST_F(PreviewsLitePageRedirectMetricsObserverTest, TestPingbackNotSent) {
  ResetTest();
  RunLitePageRedirectTest(nullptr, net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  NavigateToUntrackedUrl();
  ValidatePreviewsStateInPingback();
}
