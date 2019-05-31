// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace supervision {
namespace {

const char kTestAccountId[] = "test-account-id";
const char kFakeAccessToken[] = "fake-access-token";

}  // namespace

class FakeOnboardingWebviewHost : mojom::OnboardingWebviewHost {
 public:
  explicit FakeOnboardingWebviewHost(
      mojom::OnboardingWebviewHostRequest request)
      : binding_(this, std::move(request)) {}

  void ExpectPresentations(const std::vector<mojom::OnboardingPresentation>&
                               expected_presentations) {
    ASSERT_EQ(presentations_.size(), expected_presentations.size());

    for (std::size_t i = 0; i < expected_presentations.size(); ++i) {
      const auto& expected_presentation = expected_presentations[i];
      const auto& presentation = presentations_[i];
      EXPECT_TRUE(presentation->Equals(expected_presentation));

      // To yield more readable errors, we also test each property individually.
      EXPECT_EQ(expected_presentation.state, presentation->state);
      EXPECT_EQ(expected_presentation.can_show_next_page,
                presentation->can_show_next_page);
      EXPECT_EQ(expected_presentation.can_show_previous_page,
                presentation->can_show_previous_page);
      EXPECT_EQ(expected_presentation.can_skip_flow,
                presentation->can_skip_flow);
    }
  }

  bool exited_flow() const { return exited_flow_; }

  const base::Optional<mojom::OnboardingPage>& page_loaded() {
    return page_loaded_;
  }

  void set_load_page_result(const mojom::OnboardingLoadPageResult& result) {
    load_page_result_ = result;
  }

 private:
  void SetPresentation(mojom::OnboardingPresentationPtr presentation) override {
    presentations_.push_back(std::move(presentation));
  }

  void LoadPage(mojom::OnboardingPagePtr page,
                LoadPageCallback callback) override {
    ASSERT_FALSE(exited_flow_);

    page_loaded_ = *page;

    std::move(callback).Run(load_page_result_.Clone());
  }

  void ExitFlow() override {
    ASSERT_FALSE(exited_flow_);

    exited_flow_ = true;
  }

  mojo::Binding<mojom::OnboardingWebviewHost> binding_;

  mojom::OnboardingLoadPageResult load_page_result_{
      net::Error::OK, kDeviceOnboardingExperimentName};

  bool exited_flow_ = false;

  std::vector<mojom::OnboardingPresentationPtr> presentations_;
  base::Optional<mojom::OnboardingPage> page_loaded_;

  DISALLOW_COPY_AND_ASSIGN(FakeOnboardingWebviewHost);
};

class OnboardingControllerBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    identity_test_env()->MakeAccountAvailable(kTestAccountId);
    identity_test_env()->SetPrimaryAccount(kTestAccountId);

    controller_impl_ = std::make_unique<OnboardingControllerImpl>(profile());

    controller_impl_->BindRequest(mojo::MakeRequest(&controller_));
  }

  void BindHostAndSetupFailedAuth() {
    BindWebviewHost();

    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

    base::RunLoop().RunUntilIdle();
  }

  void BindHostAndReturnLoadPageSuccess() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = kDeviceOnboardingExperimentName;

    BindWebviewHostAndReturnResult(result);
  }

  void BindHostAndReturnLoadPageError() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::ERR_FAILED;
    result.custom_header_value = kDeviceOnboardingExperimentName;

    BindWebviewHostAndReturnResult(result);
  }

  void BindHostAndReturnMissingCustomHeader() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = base::nullopt;

    BindWebviewHostAndReturnResult(result);
  }

  void BindHostAndReturnWrongCustomHeader() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = "clearly-wrong-header-value";

    BindWebviewHostAndReturnResult(result);
  }

  Profile* profile() { return profile_.get(); }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  FakeOnboardingWebviewHost* webview_host() { return webview_host_.get(); }

 private:
  void BindWebviewHost() {
    mojom::OnboardingWebviewHostPtr webview_host_proxy;
    webview_host_ = std::make_unique<FakeOnboardingWebviewHost>(
        mojo::MakeRequest(&webview_host_proxy));

    controller_->BindWebviewHost(std::move(webview_host_proxy));
  }

  void BindWebviewHostAndReturnResult(
      const mojom::OnboardingLoadPageResult& result) {
    BindWebviewHost();
    webview_host()->set_load_page_result(result);
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kFakeAccessToken,
            base::Time::Now() + base::TimeDelta::FromHours(1));

    base::RunLoop().RunUntilIdle();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<OnboardingControllerImpl> controller_impl_;
  mojom::OnboardingControllerPtr controller_;
  std::unique_ptr<FakeOnboardingWebviewHost> webview_host_;
};

class OnboardingControllerFlowDisabledTest
    : public OnboardingControllerBaseTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kSupervisionOnboardingScreens);

    OnboardingControllerBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnboardingControllerFlowDisabledTest, ExitFlowWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerFlowDisabledTest,
       PresentOnlyLoadingStateWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerFlowDisabledTest,
       SetEligibleForKioskNextWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

class OnboardingControllerTest : public OnboardingControllerBaseTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSupervisionOnboardingScreens);

    OnboardingControllerBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnboardingControllerTest, RequestWebviewHostToLoadStartPageCorrectly) {
  BindHostAndReturnLoadPageSuccess();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://families.google.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->url_filter_pattern,
            "https://families.google.com/*");
}

TEST_F(OnboardingControllerTest, OverridePageUrlsByCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kSupervisionOnboardingUrlPrefix,
      "https://example.com/");

  BindHostAndReturnLoadPageSuccess();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://example.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->url_filter_pattern,
            "https://example.com/*");
}

TEST_F(OnboardingControllerTest, StayInFlowWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_FALSE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, PresentReadyStateWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation ready;
  ready.state = mojom::OnboardingPresentationState::kReady;
  ready.can_show_next_page = true;
  ready.can_skip_flow = true;

  webview_host()->ExpectPresentations({loading, ready});
}

TEST_F(OnboardingControllerTest, SetEligibleForKioskNextWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowOnAuthError) {
  BindHostAndSetupFailedAuth();

  EXPECT_FALSE(webview_host()->page_loaded().has_value());
  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, PresentOnlyLoadingStateOnAuthError) {
  BindHostAndSetupFailedAuth();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;
  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest, SetNotEligibleForKioskNextOnAuthError) {
  BindHostAndSetupFailedAuth();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, PresentOnlyLoadingStateOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;
  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest, SetNotEligibleForKioskNextOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest,
       PresentOnlyLoadingStateWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest,
       SetNotEligibleForKioskNextWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest,
       PresentOnlyLoadingStateWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest,
       SetNotEligibleForKioskNextWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

}  // namespace supervision
}  // namespace chromeos
