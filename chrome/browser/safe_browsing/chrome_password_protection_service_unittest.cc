// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {
const char kPhishingURL[] = "http://phishing.com";
}

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  explicit MockSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service) {}

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

  void InvokeOnBlockingPageComplete(
      const security_interstitials::UnsafeResource::UrlCheckCallback&
          callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!callback.is_null())
      callback.Run(false);
  }

 protected:
  virtual ~MockSafeBrowsingUIManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

class MockChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit MockChromePasswordProtectionService(Profile* profile)
      : ChromePasswordProtectionService(profile),
        is_incognito_(false),
        is_extended_reporting_(false),
        is_history_sync_enabled_(false) {}
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }
  bool IsHistorySyncEnabled() override { return is_history_sync_enabled_; }

  // Configures the results returned by IsExtendedReporting(), IsIncognito(),
  // and IsHistorySyncEnabled().
  void ConfigService(bool is_incognito,
                     bool is_extended_reporting,
                     bool is_history_sync_enabled) {
    is_incognito_ = is_incognito;
    is_extended_reporting_ = is_extended_reporting;
    is_history_sync_enabled_ = is_history_sync_enabled;
  }

  void SetUIManager(scoped_refptr<SafeBrowsingUIManager> ui_manager) {
    ui_manager_ = ui_manager;
  }

  MockSafeBrowsingUIManager* ui_manager() {
    return static_cast<MockSafeBrowsingUIManager*>(ui_manager_.get());
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    LoginReputationClientResponse* verdict,
                    const base::Time& receive_time) override {}

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
  bool is_history_sync_enabled_;
};

class ChromePasswordProtectionServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  typedef std::map<std::string, std::string> Parameters;

  ChromePasswordProtectionServiceTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    service_ = base::MakeUnique<MockChromePasswordProtectionService>(profile());
    service_->SetUIManager(new testing::StrictMock<MockSafeBrowsingUIManager>(
        SafeBrowsingService::CreateSafeBrowsingService()));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    service_.reset();
    request_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Sets up Finch trial feature parameters.
  void SetFeatureParams(const base::Feature& feature,
                        const std::string& trial_name,
                        const Parameters& params) {
    static std::set<std::string> features = {feature.name};
    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(trial_name,
                                                              params, features);
  }

  // Creates Finch trial parameters.
  Parameters CreateParameters(bool allowed_for_incognito,
                              bool allowed_for_all,
                              bool allowed_for_extended_reporting,
                              bool allowed_for_history_sync) {
    return {{"incognito", allowed_for_incognito ? "true" : "false"},
            {"all_population", allowed_for_all ? "true" : "false"},
            {"extended_reporting",
             allowed_for_extended_reporting ? "true" : "false"},
            {"history_sync", allowed_for_history_sync ? "true" : "false"}};
  }

  void InitializeRequest(LoginReputationClientRequest::TriggerType type) {
    request_ = new PasswordProtectionRequest(web_contents(), GURL(kPhishingURL),
                                             GURL(), GURL(), std::string(),
                                             type, true, service_.get(), 0);
  }

  void InitializeVerdict(LoginReputationClientResponse::VerdictType type) {
    verdict_ = base::MakeUnique<LoginReputationClientResponse>();
    verdict_->set_verdict_type(type);
  }

  void RequestFinished(
      PasswordProtectionRequest* request,
      std::unique_ptr<LoginReputationClientResponse> response) {
    service_->RequestFinished(request, false, std::move(response));
  }

 protected:
  variations::testing::VariationParamsManager params_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockChromePasswordProtectionService> service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  std::unique_ptr<LoginReputationClientResponse> verdict_;
};

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingSBEROnlyNoIncognito) {
  PasswordProtectionService::RequestOutcome reason;

  // By default kPasswordFieldOnFocusPinging feature is disabled.
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_FEATURE_DISABLED,
            reason);

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "SBEROnlyNoIncognito",
  // "params": {
  //     "incognito": "false",
  //     "extended_reporting": "true",
  //     "history_sync": "false"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters sber_and_no_incognito =
      CreateParameters(false, false, true, false);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "SBEROnlyNoIncognito",
                   sber_and_no_incognito);

  service_->ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION, reason);

  service_->ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION, reason);

  service_->ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingSBERAndHistorySyncNoIncognito) {
  PasswordProtectionService::RequestOutcome reason;

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "SBERAndHistorySyncNoIncognito",
  // "params": {
  //     "incognito": "false",
  //     "extended_reporting": "true",
  //     "history_sync": "true"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters sber_and_sync_no_incognito =
      CreateParameters(false, false, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging,
                   "SBERAndHistorySyncNoIncognito", sber_and_sync_no_incognito);
  service_->ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION, reason);

  service_->ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingAllButNoIncognito) {
  PasswordProtectionService::RequestOutcome reason;

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "AllButNoIncognito",
  // "params": {
  //     "all_population": "true",
  //     "incongito": "false"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters all_users = CreateParameters(false, true, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "AllButNoIncognito",
                   all_users);
  service_->ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingAll) {
  PasswordProtectionService::RequestOutcome reason;

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "All",
  // "params": {
  //     "all_population": "true",
  //     "incognito": "true"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters all_users = CreateParameters(true, true, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "All", all_users);
  service_->ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  service_->ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
}

TEST_F(ChromePasswordProtectionServiceTest,
       ShowInterstitialOnPasswordOnFocusPhishingVerdict) {
  // Enables kPasswordProtectionInterstitial feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordProtectionInterstitial);

  InitializeRequest(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE);
  InitializeVerdict(LoginReputationClientResponse::PHISHING);

  security_interstitials::UnsafeResource resource;
  EXPECT_CALL(*service_->ui_manager(), DisplayBlockingPage(testing::_))
      .WillOnce(testing::SaveArg<0>(&resource));
  RequestFinished(request_.get(), std::move(verdict_));
  EXPECT_EQ(GURL(kPhishingURL), resource.url);
  EXPECT_EQ(GURL(kPhishingURL), resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING,
            resource.threat_type);
  EXPECT_EQ(ThreatSource::PASSWORD_PROTECTION_SERVICE, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                     service_->ui_manager(), resource.callback));
}

TEST_F(ChromePasswordProtectionServiceTest, NoInterstitialOnOtherVerdicts) {
  // Enables kPasswordProtectionInterstitial feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordProtectionInterstitial);

  // For password on focus request, no interstitial shown if verdict is
  // LOW_REPUTATION.
  InitializeRequest(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE);
  InitializeVerdict(LoginReputationClientResponse::LOW_REPUTATION);

  security_interstitials::UnsafeResource resource;
  EXPECT_CALL(*service_->ui_manager(), DisplayBlockingPage(testing::_))
      .Times(0);
  RequestFinished(request_.get(), std::move(verdict_));

  // For password on focus request, no interstitial shown if verdict is
  // SAFE.
  InitializeVerdict(LoginReputationClientResponse::SAFE);
  RequestFinished(request_.get(), std::move(verdict_));

  // For protected password entry request, no interstitial shown if verdict is
  // PHISHING.
  InitializeRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  InitializeVerdict(LoginReputationClientResponse::PHISHING);
  RequestFinished(request_.get(), std::move(verdict_));

  // For protected password entry request, no interstitial shown if verdict is
  // LOW_REPUTATION.
  InitializeVerdict(LoginReputationClientResponse::LOW_REPUTATION);
  RequestFinished(request_.get(), std::move(verdict_));

  // For protected password entry request, no interstitial shown if verdict is
  // SAFE.
  InitializeVerdict(LoginReputationClientResponse::SAFE);
  RequestFinished(request_.get(), std::move(verdict_));
}

}  // namespace safe_browsing
