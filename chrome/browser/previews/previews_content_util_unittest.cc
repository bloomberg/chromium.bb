// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_content_util.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// Creates and populates a MockNavigationHandle to pass to
// DetermineAllowedClientPreveiwsState.
content::PreviewsState CallDetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    bool previews_triggering_logic_already_ran,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  EXPECT_TRUE(!navigation_handle);
  content::MockNavigationHandle mock_navigation_handle;
  mock_navigation_handle.set_url(url);
  if (is_reload) {
    mock_navigation_handle.set_reload_type(content::ReloadType::NORMAL);
  } else {
    mock_navigation_handle.set_reload_type(content::ReloadType::NONE);
  }

  return DetermineAllowedClientPreviewsState(
      previews_data, previews_triggering_logic_already_ran, is_data_saver_user,
      previews_decider, &mock_navigation_handle);
}

// A test implementation of PreviewsDecider that simply returns whether the
// preview type feature is enabled (ignores ECT and blacklist considerations).
class PreviewEnabledPreviewsDecider : public PreviewsDecider {
 public:
  PreviewEnabledPreviewsDecider() {}
  ~PreviewEnabledPreviewsDecider() override {}

  bool ShouldAllowPreviewAtNavigationStart(PreviewsUserData* previews_data,
                                           const GURL& url,
                                           bool is_reload,
                                           PreviewsType type) const override {
    return IsEnabled(type);
  }

  bool ShouldCommitPreview(PreviewsUserData* previews_data,
                           const GURL& url,
                           PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::NOSCRIPT ||
                type == PreviewsType::RESOURCE_LOADING_HINTS);
    return IsEnabled(type);
  }

  bool LoadPageHints(const GURL& url) override {
    return url.host_piece().ends_with("hintcachedhost.com");
  }

  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const override {
    return false;
  }

  void LogHintCacheMatch(const GURL& url, bool is_committed) const override {}

 private:
  bool IsEnabled(PreviewsType type) const {
    switch (type) {
      case previews::PreviewsType::OFFLINE:
        return params::IsOfflinePreviewsEnabled();
      case previews::PreviewsType::LOFI:
        return params::IsClientLoFiEnabled();
      case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
        return false;
      case previews::PreviewsType::NOSCRIPT:
        return params::IsNoScriptPreviewsEnabled();
      case previews::PreviewsType::RESOURCE_LOADING_HINTS:
        return params::IsResourceLoadingHintsEnabled();
      case previews::PreviewsType::LITE_PAGE_REDIRECT:
        return params::IsLitePageServerPreviewsEnabled();
      case PreviewsType::LITE_PAGE:
      case PreviewsType::NONE:
      case PreviewsType::UNSPECIFIED:
      case PreviewsType::LAST:
        break;
    }
    NOTREACHED();
    return false;
  }
};

class PreviewsContentUtilTest : public testing::Test {
 public:
  PreviewsContentUtilTest() {}
  ~PreviewsContentUtilTest() override {}

  PreviewsDecider* enabled_previews_decider() {
    return &enabled_previews_decider_;
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

 private:
  PreviewEnabledPreviewsDecider enabled_previews_decider_;
};

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStatePreviewsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "ClientLoFi,ResourceLoadingHints,NoScriptPreviews" /* enable_features */,
      "Previews" /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateDataSaverDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,ResourceLoadingHints,NoScriptPreviews",
      {} /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON | content::NOSCRIPT_ON,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  is_data_saver_user = false;
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateOfflineAndRedirects) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews", "ClientLoFi,ResourceLoadingHints,NoScriptPreviews");
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(content::OFFLINE_PAGE_ON,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  EXPECT_FALSE(user_data.is_redirect());
  user_data.set_allowed_previews_state(content::OFFLINE_PAGE_ON);
  previews_triggering_logic_already_ran = true;
  EXPECT_EQ(content::OFFLINE_PAGE_ON,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  EXPECT_TRUE(user_data.is_redirect());
  user_data.set_allowed_previews_state(content::PREVIEWS_OFF);
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  previews_triggering_logic_already_ran = false;
  EXPECT_EQ(content::OFFLINE_PAGE_ON,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, DetermineAllowedClientPreviewsStateClientLoFi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi", std::string());
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::CallDetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  previews_triggering_logic_already_ran, is_data_saver_user,
                  enabled_previews_decider(), nullptr));
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::CallDetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), is_reload,
                  previews_triggering_logic_already_ran, is_data_saver_user,
                  enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateResourceLoadingHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ResourceLoadingHints",
                                          std::string());
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_LT(0,
            content::RESOURCE_LOADING_HINTS_ON &
                previews::CallDetermineAllowedClientPreviewsState(
                    &user_data, GURL("https://www.google.com"), is_reload,
                    previews_triggering_logic_already_ran, is_data_saver_user,
                    enabled_previews_decider(), nullptr));
  EXPECT_LT(0,
            content::RESOURCE_LOADING_HINTS_ON &
                previews::CallDetermineAllowedClientPreviewsState(
                    &user_data, GURL("http://www.google.com"), is_reload,
                    previews_triggering_logic_already_ran, is_data_saver_user,
                    enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateNoScriptAndClientLoFi) {
  // Enable both Client LoFi and NoScript.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews", std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  // Verify both are enabled.
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::CallDetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  previews_triggering_logic_already_ran, is_data_saver_user,
                  enabled_previews_decider(), nullptr));
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::CallDetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), is_reload,
                  previews_triggering_logic_already_ran, is_data_saver_user,
                  enabled_previews_decider(), nullptr));

  // Verify non-HTTP[S] URL has no previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("data://someblob"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateLitePageRedirect) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,LitePageServerPreviews",
                                          std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  // Verify preview is enabled on HTTPS.
  EXPECT_TRUE(content::LITE_PAGE_REDIRECT_ON &
              previews::CallDetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  previews_triggering_logic_already_ran, is_data_saver_user,
                  enabled_previews_decider(), nullptr));
  // Verify non-HTTP[S] URL has no previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("data://someblob"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));

  // Other checks are performed in browser tests due to the nature of needing
  // fully initialized browser state.
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateLitePageRedirectAndPageHintPreviews) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,LitePageServerPreviews,ResourceLoadingHints,NoScriptPreviews",
      std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  // Verify Lite Page Redirect enabled for host without page hints.
  content::PreviewsState ps1 =
      previews::CallDetermineAllowedClientPreviewsState(
          &user_data, GURL("https://www.google.com"), is_reload,
          previews_triggering_logic_already_ran, is_data_saver_user,
          enabled_previews_decider(), nullptr);
  EXPECT_TRUE(ps1 & content::LITE_PAGE_REDIRECT_ON);
  EXPECT_TRUE(ps1 & content::RESOURCE_LOADING_HINTS_ON);
  EXPECT_TRUE(ps1 & content::NOSCRIPT_ON);

  // Verify only page hint client previews enabled with known page hints.
  content::PreviewsState ps2 =
      previews::CallDetermineAllowedClientPreviewsState(
          &user_data, GURL("https://www.hintcachedhost.com"), is_reload,
          previews_triggering_logic_already_ran, is_data_saver_user,
          enabled_previews_decider(), nullptr);
  EXPECT_FALSE(ps2 & content::LITE_PAGE_REDIRECT_ON);
  EXPECT_TRUE(ps2 & content::RESOURCE_LOADING_HINTS_ON);
  EXPECT_TRUE(ps2 & content::NOSCRIPT_ON);

  {
    // Now set parameter to override page hints.
    std::map<std::string, std::string> parameters;
    parameters["override_pagehints"] = "true";
    base::test::ScopedFeatureList nested_feature_list;
    nested_feature_list.InitAndEnableFeatureWithParameters(
        features::kLitePageServerPreviews, parameters);

    // Verify Lite Page Redirect now enabled for host with page hints.
    content::PreviewsState ps =
        previews::CallDetermineAllowedClientPreviewsState(
            &user_data, GURL("https://www.hintcachedhost.com"), is_reload,
            previews_triggering_logic_already_ran, is_data_saver_user,
            enabled_previews_decider(), nullptr);
    EXPECT_TRUE(ps & content::LITE_PAGE_REDIRECT_ON);
    EXPECT_TRUE(ps & content::RESOURCE_LOADING_HINTS_ON);
    EXPECT_TRUE(ps & content::NOSCRIPT_ON);
  }
}

TEST_F(PreviewsContentUtilTest, DetermineCommittedClientPreviewsState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews,ResourceLoadingHints",
      std::string());
  PreviewsUserData user_data(1);
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::HistogramTester histogram_tester;

  // Server bits take precedence over NoScript:
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LitePage",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 1);

  content::PreviewsState lite_page_redirect_enabled =
      content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
      content::RESOURCE_LOADING_HINTS_ON | content::LITE_PAGE_REDIRECT_ON;

  // LITE_PAGE_REDIRECT takes precedence over NoScript, Resource Loading Hints,
  // and Client LoFi when the committed URL is for the lite page previews
  // server.
  EXPECT_EQ(
      content::LITE_PAGE_REDIRECT_ON,
      previews::DetermineCommittedClientPreviewsState(
          &user_data, GURL("https://litepages.googlezip.net/?u=google.com"),
          lite_page_redirect_enabled, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LitePageRedirect",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 2);

  // Verify LITE_PAGE_REDIRECT_ON not committed for non-lite-page-sever URL.
  EXPECT_NE(
      content::LITE_PAGE_REDIRECT_ON,
      previews::DetermineCommittedClientPreviewsState(
          &user_data, GURL("https://www.google.com"),
          lite_page_redirect_enabled, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.ResourceLoadingHints",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 3);

  // NoScript has precedence over Client LoFi - kept for committed HTTPS:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.NoScript",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 4);

  // Try different navigation ECT value.
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // RESOURCE_LOADING_HINTS has precedence over Client LoFi and NoScript.
  EXPECT_EQ(content::RESOURCE_LOADING_HINTS_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectBucketCount(
      "Previews.Triggered.EffectiveConnectionType2.ResourceLoadingHints",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 5);

  // NoScript has precedence over Client LoFi - except for committed HTTP:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("http://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LoFi",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 6);

  // Only Client LoFi:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LoFi",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 2);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 7);

  // Only NoScript:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::NOSCRIPT_ON, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineCommittedClientPreviewsStateNoScriptCheckIfStillAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi",
                                          "NoScriptPreviews");
  PreviewsUserData user_data(1);
  // NoScript not allowed at commit time so Client LoFi chosen:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, GetMainFramePreviewsType) {
  // Simple cases:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(content::SERVER_LITE_PAGE_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::SERVER_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON));
  EXPECT_EQ(
      previews::PreviewsType::RESOURCE_LOADING_HINTS,
      previews::GetMainFramePreviewsType(content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE_REDIRECT,
            previews::GetMainFramePreviewsType(content::LITE_PAGE_REDIRECT_ON));

  // NONE cases:
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_UNSPECIFIED));
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_NO_TRANSFORM));

  // Precedence cases when server preview is available:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(
                content::SERVER_LOFI_ON | content::NOSCRIPT_ON |
                content::CLIENT_LOFI_ON | content::RESOURCE_LOADING_HINTS_ON));

  // Precedence cases when server preview is not available:
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON |
                                               content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::RESOURCE_LOADING_HINTS,
            previews::GetMainFramePreviewsType(
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(
      previews::PreviewsType::LITE_PAGE_REDIRECT,
      previews::GetMainFramePreviewsType(
          content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
          content::RESOURCE_LOADING_HINTS_ON | content::LITE_PAGE_REDIRECT_ON));
}

}  // namespace

}  // namespace previews
