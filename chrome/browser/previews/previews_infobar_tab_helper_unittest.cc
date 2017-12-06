// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_tab_helper.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/offline_pages/features/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/previews/core/previews_user_data.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_util.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class PreviewsInfoBarTabHelperUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
// Insert an OfflinePageTabHelper before PreviewsInfoBarTabHelper.
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
    offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
    InfoBarService::CreateForWebContents(web_contents());
    PreviewsInfoBarTabHelper::CreateForWebContents(web_contents());
    test_handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(kTestUrl), main_rfh());
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .SkipSettingsInitialization()
            .Build();

    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());

    PrefRegistrySimple* registry =
        drp_test_context_->pref_service()->registry();
    registry->RegisterDictionaryPref(proxy_config::prefs::kProxy);
    data_reduction_proxy_settings
        ->set_data_reduction_proxy_enabled_pref_name_for_test(
            drp_test_context_->GetDataReductionProxyEnabledPrefName());
    data_reduction_proxy_settings->InitDataReductionProxySettings(
        drp_test_context_->io_data(), drp_test_context_->pref_service(),
        drp_test_context_->request_context_getter(),
        base::WrapUnique(new data_reduction_proxy::DataStore()),
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetPreviewsState(content::PreviewsState previews_state) {
    ChromeNavigationData* nav_data =
        static_cast<ChromeNavigationData*>(test_handle_->GetNavigationData());
    if (nav_data) {
      nav_data->set_previews_state(previews_state);
      return;
    }
    std::unique_ptr<ChromeNavigationData> chrome_nav_data(
        new ChromeNavigationData());
    chrome_nav_data->set_previews_state(previews_state);
    content::WebContentsTester::For(web_contents())
        ->SetNavigationData(test_handle_.get(), std::move(chrome_nav_data));
  }

  void SimulateWillProcessResponseWithProxyHeader() {
    std::string headers = base::StringPrintf(
        "HTTP/1.1 200 OK\n%s: %s\n\n",
        data_reduction_proxy::chrome_proxy_content_transform_header(),
        data_reduction_proxy::lite_page_directive());
    test_handle_->CallWillProcessResponseForTesting(
        main_rfh(),
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    SimulateCommit();
  }

  void SimulateWillProcessResponse() {
    std::string headers("HTTP/1.1 200 OK\n\n");
    test_handle_->CallWillProcessResponseForTesting(
        main_rfh(),
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    SimulateCommit();
  }

  void SimulateCommit() {
    test_handle_->CallDidCommitNavigationForTesting(GURL(kTestUrl));
  }

  void CallDidFinishNavigation() { test_handle_.reset(); }

  void set_previews_user_data(
      std::unique_ptr<previews::PreviewsUserData> previews_user_data) {
    EXPECT_TRUE(test_handle_);
    EXPECT_TRUE(previews_user_data);
    // Store Previews information for this navigation.
    ChromeNavigationData* nav_data =
        static_cast<ChromeNavigationData*>(test_handle_->GetNavigationData());
    if (nav_data) {
      nav_data->set_previews_user_data(std::move(previews_user_data));
      return;
    }
    std::unique_ptr<ChromeNavigationData> navigation_data =
        base::MakeUnique<ChromeNavigationData>();
    navigation_data->set_previews_user_data(std::move(previews_user_data));
    content::WebContentsTester::For(web_contents())
        ->SetNavigationData(test_handle_.get(), std::move(navigation_data));
  }

 private:
  std::unique_ptr<content::NavigationHandle> test_handle_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
};

TEST_F(PreviewsInfoBarTabHelperUnitTest, CreateLitePageInfoBar) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());

  SetPreviewsState(content::SERVER_LITE_PAGE_ON);
  SimulateWillProcessResponseWithProxyHeader();
  CallDidFinishNavigation();

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  EXPECT_EQ(1U, infobar_service->infobar_count());
  EXPECT_TRUE(infobar_tab_helper->displayed_preview_infobar());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());
}

TEST_F(PreviewsInfoBarTabHelperUnitTest, NoLitePageInfoBarWithoutProxyHeader) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());

  SetPreviewsState(content::SERVER_LITE_PAGE_ON);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  EXPECT_EQ(0U, infobar_service->infobar_count());
  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());
}

TEST_F(PreviewsInfoBarTabHelperUnitTest, CreateNoScriptPreviewsInfoBar) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());

  SetPreviewsState(content::SERVER_LITE_PAGE_ON | content::NOSCRIPT_ON);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  EXPECT_EQ(1U, infobar_service->infobar_count());
  EXPECT_TRUE(infobar_tab_helper->displayed_preview_infobar());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());
}

TEST_F(PreviewsInfoBarTabHelperUnitTest, TestPreviewsIDSet) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents());

  SimulateCommit();

  uint64_t id = 5u;
  std::unique_ptr<previews::PreviewsUserData> previews_user_data =
      base::MakeUnique<previews::PreviewsUserData>(id);
  set_previews_user_data(std::move(previews_user_data));

  CallDidFinishNavigation();
  EXPECT_TRUE(infobar_tab_helper->previews_user_data());
  EXPECT_EQ(id, infobar_tab_helper->previews_user_data()->page_id());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(infobar_tab_helper->previews_user_data());
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(PreviewsInfoBarTabHelperUnitTest, CreateOfflineInfoBar) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());

  SimulateCommit();
  offline_pages::OfflinePageItem item;
  item.url = GURL(kTestUrl);
  offline_pages::OfflinePageHeader header;
  offline_pages::OfflinePageTabHelper::FromWebContents(web_contents())
      ->SetOfflinePage(item, header, true);
  CallDidFinishNavigation();

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  EXPECT_EQ(1U, infobar_service->infobar_count());
  EXPECT_TRUE(infobar_tab_helper->displayed_preview_infobar());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(infobar_tab_helper->displayed_preview_infobar());
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
