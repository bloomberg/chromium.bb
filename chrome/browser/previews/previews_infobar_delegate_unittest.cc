// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

const char kTestUrl[] = "http://www.test.com/";

// Key of the UMA Previews.InfoBarAction.LoFi histogram.
const char kUMAPreviewsInfoBarActionLoFi[] = "Previews.InfoBarAction.LoFi";

// Key of the UMA Previews.InfoBarAction.Offline histogram.
const char kUMAPreviewsInfoBarActionOffline[] =
    "Previews.InfoBarAction.Offline";

// Key of the UMA Previews.InfoBarAction.LitePage histogram.
const char kUMAPreviewsInfoBarActionLitePage[] =
    "Previews.InfoBarAction.LitePage";

class TestPreviewsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TestPreviewsWebContentsObserver> {
 public:
  explicit TestPreviewsWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        last_navigation_reload_type_(content::ReloadType::NONE) {}
  ~TestPreviewsWebContentsObserver() override {}

  content::ReloadType last_navigation_reload_type() {
    return last_navigation_reload_type_;
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_reload_type_ = navigation_handle->GetReloadType();
  }

 private:
  content::ReloadType last_navigation_reload_type_;
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TestPreviewsWebContentsObserver);

class PreviewsInfoBarDelegateUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
    PreviewsInfoBarTabHelper::CreateForWebContents(web_contents());
    TestPreviewsWebContentsObserver::CreateForWebContents(web_contents());

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

  ConfirmInfoBarDelegate* CreateInfoBar(
      PreviewsInfoBarDelegate::PreviewsInfoBarType type,
      bool is_data_saver_user) {
    PreviewsInfoBarDelegate::Create(
        web_contents(), type, is_data_saver_user,
        base::Bind(&PreviewsInfoBarDelegateUnitTest::OnDismissPreviewsInfobar,
                   base::Unretained(this)));

    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
    EXPECT_EQ(1U, infobar_service->infobar_count());

    return infobar_service->infobar_at(0)
        ->delegate()
        ->AsConfirmInfoBarDelegate();
  }

  void OnDismissPreviewsInfobar(bool user_opt_out) {
    user_opt_out_ = user_opt_out;
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;

  base::Optional<bool> user_opt_out_;
};

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestNavigationDismissal) {
  base::HistogramTester tester;

  CreateInfoBar(PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  // Try showing a second infobar. Another should not be shown since the page
  // has not navigated.
  PreviewsInfoBarDelegate::Create(
      web_contents(), PreviewsInfoBarDelegate::LOFI,
      true /* is_data_saver_user */,
      PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback());
  EXPECT_EQ(1U, infobar_service()->infobar_count());

  // Navigate and make sure the infobar is dismissed.
  NavigateAndCommit(GURL(kTestUrl));
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  EXPECT_FALSE(user_opt_out_.value());

  tester.ExpectBucketCount(
      kUMAPreviewsInfoBarActionLoFi,
      PreviewsInfoBarDelegate::INFOBAR_DISMISSED_BY_NAVIGATION, 1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestReloadDismissal) {
  base::HistogramTester tester;

  // Navigate to test URL, so we can reload later.
  NavigateAndCommit(GURL(kTestUrl));

  CreateInfoBar(PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  // Try showing a second infobar. Another should not be shown since the page
  // has not navigated.
  PreviewsInfoBarDelegate::Create(
      web_contents(), PreviewsInfoBarDelegate::LOFI,
      true /* is_data_saver_user */,
      PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback());
  EXPECT_EQ(1U, infobar_service()->infobar_count());

  // Navigate to test URL as a reload to dismiss the infobar.
  controller().LoadURL(GURL(kTestUrl), content::Referrer(),
                       ui::PAGE_TRANSITION_RELOAD, std::string());
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  EXPECT_EQ(0U, infobar_service()->infobar_count());
  EXPECT_FALSE(user_opt_out_.value());

  tester.ExpectBucketCount(kUMAPreviewsInfoBarActionLoFi,
                           PreviewsInfoBarDelegate::INFOBAR_DISMISSED_BY_RELOAD,
                           1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestUserDismissal) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  // Simulate dismissing the infobar.
  infobar->InfoBarDismissed();
  infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  tester.ExpectBucketCount(kUMAPreviewsInfoBarActionLoFi,
                           PreviewsInfoBarDelegate::INFOBAR_DISMISSED_BY_USER,
                           1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
  EXPECT_FALSE(user_opt_out_.value());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestClickLinkLoFi) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  // Simulate clicking the infobar link.
  if (infobar->LinkClicked(WindowOpenDisposition::CURRENT_TAB))
    infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  tester.ExpectBucketCount(
      kUMAPreviewsInfoBarActionLoFi,
      PreviewsInfoBarDelegate::INFOBAR_LOAD_ORIGINAL_CLICKED, 1);
  EXPECT_EQ(1, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
  EXPECT_TRUE(user_opt_out_.value());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestClickLinkLitePage) {
  base::HistogramTester tester;

  NavigateAndCommit(GURL(kTestUrl));

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LITE_PAGE, true /* is_data_saver_user */);

  // Simulate clicking the infobar link.
  if (infobar->LinkClicked(WindowOpenDisposition::CURRENT_TAB))
    infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  tester.ExpectBucketCount(
      kUMAPreviewsInfoBarActionLitePage,
      PreviewsInfoBarDelegate::INFOBAR_LOAD_ORIGINAL_CLICKED, 1);

  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  EXPECT_EQ(content::ReloadType::DISABLE_LOFI_MODE,
            TestPreviewsWebContentsObserver::FromWebContents(web_contents())
                ->last_navigation_reload_type());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestShownOncePerNavigation) {
  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  // Simulate dismissing the infobar.
  infobar->InfoBarDismissed();
  infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  PreviewsInfoBarDelegate::Create(
      web_contents(), PreviewsInfoBarDelegate::LOFI,
      true /* is_data_saver_user */,
      PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback());

  // Infobar should not be shown again since a navigation hasn't happened.
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  // Navigate and show infobar again.
  NavigateAndCommit(GURL(kTestUrl));
  CreateInfoBar(PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);
}

TEST_F(PreviewsInfoBarDelegateUnitTest, LoFiInfobarTest) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LOFI, true /* is_data_saver_user */);

  tester.ExpectUniqueSample(kUMAPreviewsInfoBarActionLoFi,
                            PreviewsInfoBarDelegate::INFOBAR_SHOWN, 1);
  EXPECT_EQ(1, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiUIShownPerSession));

  ASSERT_TRUE(infobar);
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE),
            infobar->GetMessageText());
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK),
            infobar->GetLinkText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, PreviewInfobarTest) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::LITE_PAGE, true /* is_data_saver_user */);

  tester.ExpectUniqueSample(kUMAPreviewsInfoBarActionLitePage,
                            PreviewsInfoBarDelegate::INFOBAR_SHOWN, 1);
  EXPECT_EQ(1, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiUIShownPerSession));

  // Check the strings.
  ASSERT_TRUE(infobar);
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE),
            infobar->GetMessageText());
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK),
            infobar->GetLinkText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarNonDataSaverUserTest) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::OFFLINE, false /* is_data_saver_user */);

  tester.ExpectUniqueSample(kUMAPreviewsInfoBarActionOffline,
                            PreviewsInfoBarDelegate::INFOBAR_SHOWN, 1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiUIShownPerSession));

  // Check the strings.
  ASSERT_TRUE(infobar);
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_FASTER_PAGE_TITLE),
            infobar->GetMessageText());
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK),
            infobar->GetLinkText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarDataSaverUserTest) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::OFFLINE, true /* is_data_saver_user */);

  tester.ExpectUniqueSample(kUMAPreviewsInfoBarActionOffline,
                            PreviewsInfoBarDelegate::INFOBAR_SHOWN, 1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiUIShownPerSession));

  // Check the strings.
  ASSERT_TRUE(infobar);
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE),
            infobar->GetMessageText());
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK),
            infobar->GetLinkText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarDisablesLoFi) {
  base::HistogramTester tester;

  NavigateAndCommit(GURL(kTestUrl));

  ConfirmInfoBarDelegate* infobar = CreateInfoBar(
      PreviewsInfoBarDelegate::OFFLINE, true /* is_data_saver_user */);

  tester.ExpectUniqueSample(kUMAPreviewsInfoBarActionOffline,
                            PreviewsInfoBarDelegate::INFOBAR_SHOWN, 1);

  // Simulate clicking the infobar link.
  if (infobar->LinkClicked(WindowOpenDisposition::CURRENT_TAB))
    infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  EXPECT_EQ(content::ReloadType::DISABLE_LOFI_MODE,
            TestPreviewsWebContentsObserver::FromWebContents(web_contents())
                ->last_navigation_reload_type());
}
