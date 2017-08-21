// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;
using SyncPasswordReuseEvent = UserEventSpecifics::SyncPasswordReuseEvent;
using PasswordReuseLookup = SyncPasswordReuseEvent::PasswordReuseLookup;

namespace safe_browsing {

namespace {

const char kPhishingURL[] = "http://phishing.com";
const char kTestAccountID[] = "account_id";
const char kTestEmail[] = "foo@example.com";

std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return base::MakeUnique<syncer::FakeUserEventService>();
}

}  // namespace

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
  explicit MockChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      scoped_refptr<SafeBrowsingUIManager> ui_manager)
      : ChromePasswordProtectionService(profile,
                                        content_setting_map,
                                        ui_manager),
        is_incognito_(false),
        is_extended_reporting_(false) {}
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }

  // Configures the results returned by IsExtendedReporting(), IsIncognito(),
  // and IsHistorySyncEnabled().
  void ConfigService(bool is_incognito, bool is_extended_reporting) {
    is_incognito_ = is_incognito;
    is_extended_reporting_ = is_extended_reporting;
  }

  void SetUIManager(scoped_refptr<SafeBrowsingUIManager> ui_manager) {
    ui_manager_ = ui_manager;
  }

  MockSafeBrowsingUIManager* ui_manager() {
    return static_cast<MockSafeBrowsingUIManager*>(ui_manager_.get());
  }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
};

class ChromePasswordProtectionServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  typedef std::map<std::string, std::string> Parameters;

  ChromePasswordProtectionServiceTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* incognito */, false /* guest_profile */,
        false /* store_last_modified */);
    service_ = base::MakeUnique<MockChromePasswordProtectionService>(
        profile(), content_setting_map_,
        new testing::StrictMock<MockSafeBrowsingUIManager>(
            SafeBrowsingService::CreateSafeBrowsingService()));
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(browser_context(),
                                      &BuildFakeUserEventService));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    service_.reset();
    request_ = nullptr;
    content_setting_map_->ShutdownOnUIThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::BrowserContext* CreateBrowserContext() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              signin::BuildTestSigninClient);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    builder.AddTestingFactory(AccountFetcherServiceFactory::GetInstance(),
                              FakeAccountFetcherServiceBuilder::BuildForTests);
    return builder.Build().release();
  }

  void EnableSyncPasswordReuseEvent() {
    scoped_feature_list_.InitAndEnableFeature(kSyncPasswordReuseEvent);
  }

  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

  void InitializeRequest(LoginReputationClientRequest::TriggerType type) {
    if (type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(), false,
          std::vector<std::string>({"somedomain.com"}), type, true,
          service_.get(), 0);
    } else {
      ASSERT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT, type);
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(), true,
          std::vector<std::string>(), type, true, service_.get(), 0);
    }
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

  void SetUpSyncAccount(const std::string& hosted_domain) {
    FakeAccountFetcherService* account_fetcher_service =
        static_cast<FakeAccountFetcherService*>(
            AccountFetcherServiceFactory::GetForProfile(profile()));
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile());
    account_fetcher_service->FakeUserInfoFetchSuccess(
        account_tracker_service->PickAccountIdForAccount(kTestAccountID,
                                                         kTestEmail),
        kTestEmail, kTestAccountID, hosted_domain, "full_name", "given_name",
        "locale", "http://picture.example.com/picture.jpg");
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<MockChromePasswordProtectionService> service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  std::unique_ptr<LoginReputationClientResponse> verdict_;
  // Owned by KeyedServiceFactory.
  syncer::FakeUserEventService* fake_user_event_service_;
};

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForPasswordOnFocusPing) {
  // Password field on focus pinging is enabled on !incognito && SBER.

  PasswordProtectionService::RequestOutcome reason;
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION, reason);

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(
      service_->IsPingingEnabled(kPasswordFieldOnFocusPinging, &reason));
  EXPECT_EQ(PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForProtectedPasswordEntryPing) {
  // Protected password entry pinging is enabled for all safe browsing users.
  PasswordProtectionService::RequestOutcome reason;

  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kProtectedPasswordEntryPinging, &reason));
  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kProtectedPasswordEntryPinging, &reason));
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kProtectedPasswordEntryPinging, &reason));
  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(
      service_->IsPingingEnabled(kProtectedPasswordEntryPinging, &reason));
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

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetSyncAccountType) {
  SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
      SigninManagerFactory::GetForProfile(profile()));
  signin_manager->SetAuthenticatedAccountInfo(kTestAccountID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound));
  EXPECT_EQ(LoginReputationClientRequest::PasswordReuseEvent::GMAIL,
            service_->GetSyncAccountType());

  SetUpSyncAccount("example.edu");
  EXPECT_EQ(LoginReputationClientRequest::PasswordReuseEvent::GSUITE,
            service_->GetSyncAccountType());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyUpdateSecurityState) {
  GURL url("http://password_reuse_url.com");
  NavigateAndCommit(url);
  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  ASSERT_FALSE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetVisibleEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);

  service_->UpdateSecurityState(SB_THREAT_TYPE_PASSWORD_REUSE, web_contents());
  ASSERT_TRUE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetVisibleEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_PASSWORD_REUSE, current_threat_type);

  service_->UpdateSecurityState(safe_browsing::SB_THREAT_TYPE_SAFE,
                                web_contents());
  current_threat_type = SB_THREAT_TYPE_UNUSED;
  ASSERT_FALSE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetVisibleEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);
  LoginReputationClientResponse verdict;
  EXPECT_EQ(
      LoginReputationClientResponse::SAFE,
      service_->GetCachedVerdict(
          url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &verdict));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseUserEventNotRecorded) {
  // Feature not enabled.
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), PasswordProtectionService::MATCHED_WHITELIST, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());

  EnableSyncPasswordReuseEvent();
  // Feature enabled but no committed navigation entry.
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), PasswordProtectionService::MATCHED_WHITELIST, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseDetectedUserEventRecorded) {
  EnableSyncPasswordReuseEvent();
  NavigateAndCommit(GURL("https://www.example.com/"));

  // Case 1: safe_browsing_enabled = true
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  SyncPasswordReuseEvent event = GetUserEventService()
                                     ->GetRecordedUserEvents()[0]
                                     .sync_password_reuse_event();
  EXPECT_TRUE(event.reuse_detected().status().enabled());

  // Case 2: safe_browsing_enabled = false
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(2ul, GetUserEventService()->GetRecordedUserEvents().size());
  event = GetUserEventService()
              ->GetRecordedUserEvents()[1]
              .sync_password_reuse_event();
  EXPECT_FALSE(event.reuse_detected().status().enabled());

  // Not checking for the extended_reporting_level since that requires setting
  // multiple prefs and doesn't add much verification value.
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseLookupUserEventRecorded) {
  EnableSyncPasswordReuseEvent();
  NavigateAndCommit(GURL("https://www.example.com/"));

  std::vector<std::pair<PasswordProtectionService::RequestOutcome,
                        PasswordReuseLookup::LookupResult>>
      test_cases_result_only = {
          {PasswordProtectionService::MATCHED_WHITELIST,
           PasswordReuseLookup::WHITELIST_HIT},
          {PasswordProtectionService::URL_NOT_VALID_FOR_REPUTATION_COMPUTING,
           PasswordReuseLookup::URL_UNSUPPORTED},
          {PasswordProtectionService::CANCELED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::TIMEDOUT,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::REQUEST_MALFORMED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::FETCH_FAILED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::RESPONSE_MALFORMED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::SERVICE_DESTROYED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::DISABLED_DUE_TO_FEATURE_DISABLED,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION,
           PasswordReuseLookup::REQUEST_FAILURE},
          {PasswordProtectionService::MAX_OUTCOME,
           PasswordReuseLookup::REQUEST_FAILURE}};

  unsigned long t = 0;
  for (const auto& it : test_cases_result_only) {
    VLOG(1) << __FUNCTION__ << ": case: " << t;
    service_->MaybeLogPasswordReuseLookupEvent(web_contents(), it.first,
                                               nullptr);
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size());
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .sync_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(it.second, reuse_lookup.lookup_result());
    t++;
  }

  {
    VLOG(1) << __FUNCTION__ << ": case: " << t;
    auto response = base::MakeUnique<LoginReputationClientResponse>();
    response->set_verdict_token("token1");
    response->set_verdict_type(LoginReputationClientResponse::LOW_REPUTATION);
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), PasswordProtectionService::RESPONSE_ALREADY_CACHED,
        response.get());
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size());
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .sync_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(PasswordReuseLookup::CACHE_HIT, reuse_lookup.lookup_result());
    EXPECT_EQ(PasswordReuseLookup::LOW_REPUTATION, reuse_lookup.verdict());
    EXPECT_EQ("token1", reuse_lookup.verdict_token());
    t++;
  }

  {
    VLOG(1) << __FUNCTION__ << ": case: " << t;
    auto response = base::MakeUnique<LoginReputationClientResponse>();
    response->set_verdict_token("token2");
    response->set_verdict_type(LoginReputationClientResponse::SAFE);
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), PasswordProtectionService::SUCCEEDED, response.get());
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size());
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .sync_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(PasswordReuseLookup::REQUEST_SUCCESS,
              reuse_lookup.lookup_result());
    EXPECT_EQ(PasswordReuseLookup::SAFE, reuse_lookup.verdict());
    EXPECT_EQ("token2", reuse_lookup.verdict_token());
    t++;
  }
}

}  // namespace safe_browsing
