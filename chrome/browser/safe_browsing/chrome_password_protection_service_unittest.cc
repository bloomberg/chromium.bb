// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
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
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;
using GaiaPasswordReuse = UserEventSpecifics::GaiaPasswordReuse;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;

namespace safe_browsing {

namespace {

const char kPhishingURL[] = "http://phishing.com";
const char kTestAccountID[] = "account_id";
const char kTestEmail[] = "foo@example.com";
const char kBasicResponseHeaders[] = "HTTP/1.1 200 OK";
const char kRedirectURL[] = "http://redirect.com";

std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return base::MakeUnique<syncer::FakeUserEventService>();
}

}  // namespace

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

  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
};

class ChromePasswordProtectionServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
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
        new SafeBrowsingUIManager(
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

  void EnableGaiaPasswordReuseReporting() {
    scoped_feature_list_.InitAndEnableFeature(kGaiaPasswordReuseReporting);
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

  void SimulateRequestFinished(
      LoginReputationClientResponse::VerdictType verdict_type) {
    std::unique_ptr<LoginReputationClientResponse> verdict =
        base::MakeUnique<LoginReputationClientResponse>();
    verdict->set_verdict_type(verdict_type);
    service_->RequestFinished(request_.get(), false, std::move(verdict));
  }

  void SetUpSyncAccount(const std::string& hosted_domain,
                        const std::string& account_id,
                        const std::string& email) {
    FakeAccountFetcherService* account_fetcher_service =
        static_cast<FakeAccountFetcherService*>(
            AccountFetcherServiceFactory::GetForProfile(profile()));
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile());
    account_fetcher_service->FakeUserInfoFetchSuccess(
        account_tracker_service->PickAccountIdForAccount(account_id, email),
        email, account_id, hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void PrepareRequest(LoginReputationClientRequest::TriggerType trigger_type,
                      bool is_warning_showing) {
    InitializeRequest(trigger_type);
    request_->set_is_modal_warning_showing(is_warning_showing);
    service_->pending_requests_.insert(request_);
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateWillStart(
      content::NavigationHandle* test_handle) {
    std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
        service_->MaybeCreateNavigationThrottle(test_handle);
    if (throttle)
      test_handle->RegisterThrottleForTesting(std::move(throttle));

    return test_handle->CallWillStartRequestForTesting(
        /*is_post=*/false, content::Referrer(), /*has_user_gesture=*/false,
        ui::PAGE_TRANSITION_LINK, /*is_external_protocol=*/false);
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

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetSyncAccountType) {
  SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
      SigninManagerFactory::GetForProfile(profile()));
  signin_manager->SetAuthenticatedAccountInfo(kTestAccountID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestAccountID), std::string(kTestEmail));
  EXPECT_EQ(LoginReputationClientRequest::PasswordReuseEvent::GMAIL,
            service_->GetSyncAccountType());

  SetUpSyncAccount("example.edu", std::string(kTestAccountID),
                   std::string(kTestEmail));
  EXPECT_EQ(LoginReputationClientRequest::PasswordReuseEvent::GSUITE,
            service_->GetSyncAccountType());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyUpdateSecurityState) {
  GURL url("http://password_reuse_url.com");
  NavigateAndCommit(url);
  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  ASSERT_FALSE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);

  // Cache a verdict for this URL.
  LoginReputationClientResponse verdict_proto;
  verdict_proto.set_verdict_type(LoginReputationClientResponse::PHISHING);
  verdict_proto.set_cache_duration_sec(600);
  verdict_proto.set_cache_expression("password_reuse_url.com/");
  service_->CacheVerdict(url,
                         LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                         &verdict_proto, base::Time::Now());

  service_->UpdateSecurityState(SB_THREAT_TYPE_PASSWORD_REUSE, web_contents());
  ASSERT_TRUE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_PASSWORD_REUSE, current_threat_type);

  service_->UpdateSecurityState(safe_browsing::SB_THREAT_TYPE_SAFE,
                                web_contents());
  current_threat_type = SB_THREAT_TYPE_UNUSED;
  service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type);
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

  EnableGaiaPasswordReuseReporting();
  // Feature enabled but no committed navigation entry.
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), PasswordProtectionService::MATCHED_WHITELIST, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseDetectedUserEventRecorded) {
  EnableGaiaPasswordReuseReporting();
  NavigateAndCommit(GURL("https://www.example.com/"));

  // Case 1: safe_browsing_enabled = true
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  GaiaPasswordReuse event = GetUserEventService()
                                ->GetRecordedUserEvents()[0]
                                .gaia_password_reuse_event();
  EXPECT_TRUE(event.reuse_detected().status().enabled());

  // Case 2: safe_browsing_enabled = false
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(2ul, GetUserEventService()->GetRecordedUserEvents().size());
  event = GetUserEventService()
              ->GetRecordedUserEvents()[1]
              .gaia_password_reuse_event();
  EXPECT_FALSE(event.reuse_detected().status().enabled());

  // Not checking for the extended_reporting_level since that requires setting
  // multiple prefs and doesn't add much verification value.
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseLookupUserEventRecorded) {
  EnableGaiaPasswordReuseReporting();
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
                                           .gaia_password_reuse_event()
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
                                           .gaia_password_reuse_event()
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
                                           .gaia_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(PasswordReuseLookup::REQUEST_SUCCESS,
              reuse_lookup.lookup_result());
    EXPECT_EQ(PasswordReuseLookup::SAFE, reuse_lookup.verdict());
    EXPECT_EQ("token2", reuse_lookup.verdict_token());
    t++;
  }
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetChangePasswordURL) {
  SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
      SigninManagerFactory::GetForProfile(profile()));
  signin_manager->SetAuthenticatedAccountInfo(kTestAccountID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestAccountID),
                   std::string(kTestEmail));
  EXPECT_EQ(GURL("https://accounts.google.com/"
                 "AccountChooser?Email=foo%40example.com&continue=https%3A%2F%"
                 "2Fmyaccount.google.com%2Fsigninoptions%2Fpassword%3Futm_"
                 "source%3DGoogle%26utm_campaign%3DPhishGuard&hl=en"),
            service_->GetChangePasswordURL());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringPasswordOnFocusPingNotBlocked) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  PrepareRequest(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                 /*is_warning_showing=*/false);
  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateWillStart(test_handle.get()));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringPasswordReusePingDeferred) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a on-going password reuse request that hasn't received
  // verdict yet.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 /*is_warning_showing=*/false);

  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  // Verify navigation get deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            SimulateWillStart(test_handle.get()));
  EXPECT_FALSE(test_handle->HasCommitted());
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a SAFE verdict.
  SimulateRequestFinished(LoginReputationClientResponse::SAFE);
  base::RunLoop().RunUntilIdle();

  // Verify that navigation can be resumed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            test_handle->CallWillProcessResponseForTesting(
                main_rfh(),
                net::HttpUtil::AssembleRawHeaders(
                    kBasicResponseHeaders, strlen(kBasicResponseHeaders))));
  test_handle->CallDidCommitNavigationForTesting(redirect_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_handle->HasCommitted());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringModalWarningCanceled) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a password reuse request, whose verdict is triggering a modal
  // warning.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 /*is_warning_showing=*/true);
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a phishing verdict.
  SimulateRequestFinished(LoginReputationClientResponse::PHISHING);
  base::RunLoop().RunUntilIdle();

  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  // Verify that navigation gets canceled.
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            SimulateWillStart(test_handle.get()));
  EXPECT_FALSE(test_handle->HasCommitted());
}

}  // namespace safe_browsing
