// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;

// If user specifically mark a site as legitimate, we will keep this decision
// for 2 days.
const int kOverrideVerdictCacheDurationSec = 2 * 24 * 60 * 60;

}  // namespace

ChromePasswordProtectionService::ChromePasswordProtectionService(
    SafeBrowsingService* sb_service,
    Profile* profile)
    : PasswordProtectionService(
          sb_service->database_manager(),
          sb_service->url_request_context(),
          HistoryServiceFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          HostContentSettingsMapFactory::GetForProfile(profile)),
      ui_manager_(sb_service->ui_manager()),
      profile_(profile),
      navigation_observer_manager_(sb_service->navigation_observer_manager()) {
  DCHECK(profile_);
}

ChromePasswordProtectionService::~ChromePasswordProtectionService() {
  if (content_settings()) {
    CleanUpExpiredVerdicts();
    UMA_HISTOGRAM_COUNTS_1000(
        "PasswordProtection.NumberOfCachedVerdictBeforeShutdown."
        "PasswordOnFocus",
        GetStoredVerdictCount(
            LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
    UMA_HISTOGRAM_COUNTS_1000(
        "PasswordProtection.NumberOfCachedVerdictBeforeShutdown."
        "ProtectedPasswordEntry",
        GetStoredVerdictCount(
            LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  }
}

void ChromePasswordProtectionService::FillReferrerChain(
    const GURL& event_url,
    int event_tab_id,
    LoginReputationClientRequest::Frame* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByEventURL(
          event_url, event_tab_id, kPasswordEventAttributionUserGestureLimit,
          frame->mutable_referrer_chain());
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.PasswordEventAttribution",
      frame->referrer_chain().size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.PasswordEventAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  return IsExtendedReportingEnabled(*profile_->GetPrefs());
}

bool ChromePasswordProtectionService::IsIncognito() {
  DCHECK(profile_);
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    const base::Feature& feature,
    RequestOutcome* reason) {
  // Don't start pinging on an invalid profile, or if user turns off Safe
  // Browsing service.
  if (!profile_ ||
      !profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return false;
  }

  DCHECK(feature.name == kProtectedPasswordEntryPinging.name ||
         feature.name == kPasswordFieldOnFocusPinging.name);
  if (!base::FeatureList::IsEnabled(feature)) {
    *reason = DISABLED_DUE_TO_FEATURE_DISABLED;
    return false;
  }

  bool allowed_incognito =
      base::GetFieldTrialParamByFeatureAsBool(feature, "incognito", false);
  if (IsIncognito() && !allowed_incognito) {
    *reason = DISABLED_DUE_TO_INCOGNITO;
    return false;
  }

  bool allowed_all_population =
      base::GetFieldTrialParamByFeatureAsBool(feature, "all_population", false);
  if (!allowed_all_population) {
    bool allowed_extended_reporting = base::GetFieldTrialParamByFeatureAsBool(
        feature, "extended_reporting", false);
    if (IsExtendedReporting() && allowed_extended_reporting)
      return true;  // Ping enabled because this user opted in extended
                    // reporting.

    bool allowed_history_sync =
        base::GetFieldTrialParamByFeatureAsBool(feature, "history_sync", false);
    if (IsHistorySyncEnabled() && allowed_history_sync)
      return true;

    *reason = DISABLED_DUE_TO_USER_POPULATION;
  }

  return allowed_all_population;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return sync && sync->IsSyncActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

PasswordProtectionService::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() {
  DCHECK(profile_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);

  if (!signin_manager)
    return LoginReputationClientRequest::PasswordReuseEvent::NOT_SIGNED_IN;

  AccountInfo account_info = signin_manager->GetAuthenticatedAccountInfo();

  if (account_info.account_id.empty() || account_info.hosted_domain.empty()) {
    return LoginReputationClientRequest::PasswordReuseEvent::NOT_SIGNED_IN;
  }

  // For gmail or googlemail account, the hosted_domain will always be
  // kNoHostedDomainFound.
  return account_info.hosted_domain ==
                 std::string(AccountTrackerService::kNoHostedDomainFound)
             ? LoginReputationClientRequest::PasswordReuseEvent::GMAIL
             : LoginReputationClientRequest::PasswordReuseEvent::GSUITE;
}

void ChromePasswordProtectionService::ShowPhishingInterstitial(
    const GURL& phishing_url,
    const std::string& token,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ui_manager_)
    return;
  security_interstitials::UnsafeResource resource;
  resource.url = phishing_url;
  resource.original_url = phishing_url;
  resource.is_subresource = false;
  resource.threat_type = SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING;
  resource.threat_source =
      safe_browsing::ThreatSource::PASSWORD_PROTECTION_SERVICE;
  resource.web_contents_getter =
      safe_browsing::SafeBrowsingUIManager::UnsafeResource::
          GetWebContentsGetter(web_contents->GetRenderProcessHost()->GetID(),
                               web_contents->GetMainFrame()->GetRoutingID());
  resource.token = token;
  if (!ui_manager_->IsWhitelisted(resource)) {
    web_contents->GetController().DiscardNonCommittedEntries();
  }
  ui_manager_->DisplayBlockingPage(resource);
}

void ChromePasswordProtectionService::UpdateSecurityState(
    safe_browsing::SBThreatType threat_type,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!ui_manager_ || !entry)
    return;

  const GURL url = entry->GetURL();
  const GURL url_with_empty_path = url.GetWithEmptyPath();
  if (threat_type == SB_THREAT_TYPE_SAFE) {
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       false /* from_pending_only */);
    // Overrides cached verdicts.
    LoginReputationClientResponse verdict;
    GetCachedVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                     &verdict);
    verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
    verdict.set_cache_duration_sec(kOverrideVerdictCacheDurationSec);
    CacheVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 &verdict, base::Time::Now());
    return;
  }

  safe_browsing::SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  // If user already click-through interstitial warning, or if there's already
  // a dangerous security state showing, we'll override it.
  if (ui_manager_->IsUrlWhitelistedOrPendingForWebContents(
          url_with_empty_path, false,
          web_contents->GetController().GetVisibleEntry(), web_contents, false,
          &current_threat_type)) {
    DCHECK_NE(SB_THREAT_TYPE_UNUSED, current_threat_type);
    if (current_threat_type == threat_type)
      return;
    // Resets previous threat type.
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       false /* from_pending_only */);
  }
  ui_manager_->AddToWhitelistUrlSet(url_with_empty_path, web_contents, true,
                                    threat_type);
}

ChromePasswordProtectionService::ChromePasswordProtectionService(
    Profile* profile,
    scoped_refptr<HostContentSettingsMap> content_setting_map,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : PasswordProtectionService(nullptr,
                                nullptr,
                                nullptr,
                                content_setting_map.get()),
      ui_manager_(ui_manager),
      profile_(profile) {}
}  // namespace safe_browsing
