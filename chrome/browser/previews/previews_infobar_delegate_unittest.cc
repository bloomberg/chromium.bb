// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
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
        last_navigation_reload_type_(content::ReloadType::NONE),
        should_have_page_id_(true) {}
  ~TestPreviewsWebContentsObserver() override {}

  content::ReloadType last_navigation_reload_type() {
    return last_navigation_reload_type_;
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ChromeNavigationData* chrome_navigation_data = new ChromeNavigationData();
    content::WebContentsTester::For(web_contents())
        ->SetNavigationData(navigation_handle,
                            base::WrapUnique(chrome_navigation_data));
    data_reduction_proxy::DataReductionProxyData* data =
        new data_reduction_proxy::DataReductionProxyData();
    chrome_navigation_data->SetDataReductionProxyData(base::WrapUnique(data));
    data->set_used_data_reduction_proxy(true);
    if (should_have_page_id_)
      data->set_page_id(1u);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_reload_type_ = navigation_handle->GetReloadType();
  }

  void set_should_have_page_id(bool should_have_page_id) {
    should_have_page_id_ = should_have_page_id;
  }

 private:
  content::ReloadType last_navigation_reload_type_;
  bool should_have_page_id_;
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TestPreviewsWebContentsObserver);

class PreviewsInfoBarDelegateUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  PreviewsInfoBarDelegateUnitTest()
      : field_trial_list_(new base::FieldTrialList(nullptr)) {}

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

    TestingBrowserProcess::GetGlobal()->SetLocalState(
        drp_test_context_->pref_service());
    network_time::NetworkTimeTracker::RegisterPrefs(registry);
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  PreviewsInfoBarDelegate* CreateInfoBar(previews::PreviewsType type,
                                         base::Time previews_freshness,
                                         bool is_data_saver_user) {
    PreviewsInfoBarDelegate::Create(
        web_contents(), type, previews_freshness, is_data_saver_user,
        base::Bind(&PreviewsInfoBarDelegateUnitTest::OnDismissPreviewsInfobar,
                   base::Unretained(this)));

    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
    EXPECT_EQ(1U, infobar_service->infobar_count());

    return static_cast<PreviewsInfoBarDelegate*>(
        infobar_service->infobar_at(0)->delegate());
  }

  void EnableStalePreviewsTimestamp() {
    const std::string kTrialName = "TrialName";
    const std::string kGroupName = "GroupName";

    std::map<std::string, std::string> variation_params;
    variation_params["min_staleness_in_minutes"] = "2";
    variation_params["max_staleness_in_minutes"] = "1440";

    base::AssociateFieldTrialParams(kTrialName, kGroupName, variation_params);
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        previews::features::kStalePreviewsTimestamp.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, field_trial);
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
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
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestNavigationDismissal) {
  base::HistogramTester tester;

  CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                true /* is_data_saver_user */);

  // Try showing a second infobar. Another should not be shown since the page
  // has not navigated.
  PreviewsInfoBarDelegate::Create(
      web_contents(), previews::PreviewsType::LOFI,
      base::Time() /* previews_freshness */, true /* is_data_saver_user */,
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

  CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                true /* is_data_saver_user */);

  // Try showing a second infobar. Another should not be shown since the page
  // has not navigated.
  PreviewsInfoBarDelegate::Create(
      web_contents(), previews::PreviewsType::LOFI,
      base::Time() /* previews_freshness */, true /* is_data_saver_user */,
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

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  EXPECT_EQ(0u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestUserDismissal) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                    true /* is_data_saver_user */);

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

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestTabClosedDismissal) {
  base::HistogramTester tester;

  CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                true /* is_data_saver_user */);

  // Delete the infobar without any other infobar actions.
  infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  tester.ExpectBucketCount(
      kUMAPreviewsInfoBarActionLoFi,
      PreviewsInfoBarDelegate::INFOBAR_DISMISSED_BY_TAB_CLOSURE, 1);
  EXPECT_EQ(0, drp_test_context_->pref_service()->GetInteger(
                   data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
  EXPECT_FALSE(user_opt_out_.value());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestClickLinkLoFi) {
  NavigateAndCommit(GURL(kTestUrl));
  const struct {
    bool using_previews_blacklist;
  } tests[] = {
      {true}, {false},
  };
  for (const auto test : tests) {
    drp_test_context_->config()->ResetLoFiStatusForTest();
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(nullptr));
    if (test.using_previews_blacklist) {
      base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyPreviewsBlackListTransition", "Enabled_");
    }
    base::HistogramTester tester;
    // Call Reload and CommitPendingNavigation to force DidFinishNavigation.
    web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

    ConfirmInfoBarDelegate* infobar =
        CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                      true /* is_data_saver_user */);

    // Simulate clicking the infobar link.
    if (infobar->LinkClicked(WindowOpenDisposition::CURRENT_TAB))
      infobar_service()->infobar_at(0)->RemoveSelf();
    EXPECT_EQ(0U, infobar_service()->infobar_count());

    tester.ExpectBucketCount(
        kUMAPreviewsInfoBarActionLoFi,
        PreviewsInfoBarDelegate::INFOBAR_LOAD_ORIGINAL_CLICKED, 1);
    EXPECT_EQ(test.using_previews_blacklist ? 0 : 1,
              drp_test_context_->pref_service()->GetInteger(
                  data_reduction_proxy::prefs::kLoFiLoadImagesPerSession));
    EXPECT_TRUE(user_opt_out_.value());

    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());
    EXPECT_EQ(1u, data_reduction_proxy_settings->data_reduction_proxy_service()
                      ->pingback_client()
                      ->OptOutsSizeForTesting());
  }
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestClickLinkLitePage) {
  base::HistogramTester tester;

  NavigateAndCommit(GURL(kTestUrl));

  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LITE_PAGE, base::Time(),
                    true /* is_data_saver_user */);

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

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  EXPECT_EQ(1u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, InfobarTestShownOncePerNavigation) {
  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                    true /* is_data_saver_user */);

  // Simulate dismissing the infobar.
  infobar->InfoBarDismissed();
  infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  PreviewsInfoBarDelegate::Create(
      web_contents(), previews::PreviewsType::LOFI,
      base::Time() /* previews_freshness */, true /* is_data_saver_user */,
      PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback());

  // Infobar should not be shown again since a navigation hasn't happened.
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  // Navigate and show infobar again.
  NavigateAndCommit(GURL(kTestUrl));
  CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                true /* is_data_saver_user */);
}

TEST_F(PreviewsInfoBarDelegateUnitTest, LoFiInfobarTest) {
  base::HistogramTester tester;

  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LOFI, base::Time(),
                    true /* is_data_saver_user */);

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

  PreviewsInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LITE_PAGE, base::Time(),
                    true /* is_data_saver_user */);

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
  ASSERT_EQ(base::string16(), infobar->GetTimestampText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarNonDataSaverUserTest) {
  base::HistogramTester tester;

  PreviewsInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::OFFLINE, base::Time(),
                    false /* is_data_saver_user */);

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
  ASSERT_EQ(base::string16(), infobar->GetTimestampText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarDataSaverUserTest) {
  base::HistogramTester tester;

  PreviewsInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::OFFLINE, base::Time(),
                    true /* is_data_saver_user */);

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
  ASSERT_EQ(base::string16(), infobar->GetTimestampText());
#if defined(OS_ANDROID)
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(PreviewsInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}

TEST_F(PreviewsInfoBarDelegateUnitTest, OfflineInfobarDisablesLoFi) {
  base::HistogramTester tester;

  TestPreviewsWebContentsObserver::FromWebContents(web_contents())
      ->set_should_have_page_id(false);

  NavigateAndCommit(GURL(kTestUrl));

  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::OFFLINE, base::Time(),
                    true /* is_data_saver_user */);

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

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  EXPECT_EQ(0u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, PingbackClientClearedTabClosed) {
  base::HistogramTester tester;

  NavigateAndCommit(GURL(kTestUrl));

  ConfirmInfoBarDelegate* infobar =
      CreateInfoBar(previews::PreviewsType::LITE_PAGE, base::Time(),
                    true /* is_data_saver_user */);

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

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  EXPECT_EQ(1u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());

  {
    std::unique_ptr<content::NavigationHandle> navigation_handle(
        content::NavigationHandle::CreateNavigationHandleForTesting(
            GURL("url"), main_rfh(), true));
  }

  SetContents(nullptr);

  EXPECT_EQ(1u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());

  drp_test_context_->RunUntilIdle();

  EXPECT_EQ(0u, data_reduction_proxy_settings->data_reduction_proxy_service()
                    ->pingback_client()
                    ->OptOutsSizeForTesting());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, PreviewInfobarTimestampMintuesTest) {
  EnableStalePreviewsTimestamp();

  int staleness_in_minutes = 5;

  PreviewsInfoBarDelegate* infobar = CreateInfoBar(
      previews::PreviewsType::LITE_PAGE,
      base::Time::Now() - base::TimeDelta::FromMinutes(staleness_in_minutes),
      true /* is_data_saver_user */);

  ASSERT_EQ(
      l10n_util::GetStringFUTF16(IDS_PREVIEWS_INFOBAR_TIMESTAMP_MINUTES,
                                 base::IntToString16(staleness_in_minutes)),
      infobar->GetTimestampText());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, PreviewInfobarTimestampHourTest) {
  EnableStalePreviewsTimestamp();

  int staleness_in_minutes = 65;

  PreviewsInfoBarDelegate* infobar = CreateInfoBar(
      previews::PreviewsType::LITE_PAGE,
      base::Time::Now() - base::TimeDelta::FromMinutes(staleness_in_minutes),
      true /* is_data_saver_user */);

  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_TIMESTAMP_ONE_HOUR),
            infobar->GetTimestampText());
}

TEST_F(PreviewsInfoBarDelegateUnitTest, PreviewInfobarTimestampHoursTest) {
  EnableStalePreviewsTimestamp();

  int staleness_in_hours = 2;

  PreviewsInfoBarDelegate* infobar = CreateInfoBar(
      previews::PreviewsType::LITE_PAGE,
      base::Time::Now() - base::TimeDelta::FromHours(staleness_in_hours),
      true /* is_data_saver_user */);

  ASSERT_EQ(l10n_util::GetStringFUTF16(IDS_PREVIEWS_INFOBAR_TIMESTAMP_HOURS,
                                       base::IntToString16(staleness_in_hours)),
            infobar->GetTimestampText());
}
