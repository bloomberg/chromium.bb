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
#include "chrome/browser/sync/user_event_service_factory.h"
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
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using sync_pb::UserEventSpecifics;
using GaiaPasswordReuse = UserEventSpecifics::GaiaPasswordReuse;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;
using SafeBrowsingStatus =
    GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus;

namespace safe_browsing {

namespace {

// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;

// If user specifically mark a site as legitimate, we will keep this decision
// for 2 days.
const int kOverrideVerdictCacheDurationSec = 2 * 24 * 60 * 60;

int64_t GetMicrosecondsSinceWindowsEpoch(base::Time time) {
  return (time - base::Time()).InMicroseconds();
}

PasswordReuseLookup::ReputationVerdict GetVerdictToLogFromResponse(
    LoginReputationClientResponse::VerdictType response_verdict) {
  switch (response_verdict) {
    case LoginReputationClientResponse::SAFE:
      return PasswordReuseLookup::SAFE;
    case LoginReputationClientResponse::LOW_REPUTATION:
      return PasswordReuseLookup::LOW_REPUTATION;
    case LoginReputationClientResponse::PHISHING:
      return PasswordReuseLookup::PHISHING;
    case LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected response_verdict: " << response_verdict;
      return PasswordReuseLookup::VERDICT_UNSPECIFIED;
  }
  NOTREACHED() << "Unexpected response_verdict: " << response_verdict;
  return PasswordReuseLookup::VERDICT_UNSPECIFIED;
}

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

// static
bool ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
    Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingChangePasswordInSettingsEnabled);
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

void ChromePasswordProtectionService::ShowModalWarning(
    content::WebContents* web_contents,
    const std::string& unused_verdict_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(jialiul): Use verdict_token field in post warning report.
  UpdateSecurityState(SB_THREAT_TYPE_PASSWORD_REUSE, web_contents);
#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
  // TODO(jialiul): Remove the restriction on Mac when this dialog has a Cocoa
  // version as well.
  ShowPasswordReuseModalWarningDialog(
      web_contents, this,
      base::BindOnce(&ChromePasswordProtectionService::OnWarningDone,
                     GetWeakPtr(), web_contents,
                     PasswordProtectionService::MODAL_DIALOG));
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER
  OnWarningShown(web_contents, PasswordProtectionService::MODAL_DIALOG);
}

void ChromePasswordProtectionService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromePasswordProtectionService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

PrefService* ChromePasswordProtectionService::GetPrefs() {
  return profile_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  return IsExtendedReportingEnabled(*GetPrefs());
}

bool ChromePasswordProtectionService::IsIncognito() {
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    const base::Feature& feature,
    RequestOutcome* reason) {
  if (!IsSafeBrowsingEnabled())
    return false;

  DCHECK(feature.name == kProtectedPasswordEntryPinging.name ||
         feature.name == kPasswordFieldOnFocusPinging.name);
  if (!base::FeatureList::IsEnabled(feature)) {
    *reason = DISABLED_DUE_TO_FEATURE_DISABLED;
    return false;
  }

  // Protected password entry pinging is enabled for all users.
  if (feature.name == kProtectedPasswordEntryPinging.name)
    return true;

  // Password field on focus pinging is enabled for !incognito &&
  // extended_reporting.
  if (IsIncognito()) {
    *reason = DISABLED_DUE_TO_INCOGNITO;
    return false;
  }
  if (!IsExtendedReporting()) {
    *reason = DISABLED_DUE_TO_USER_POPULATION;
    return false;
  }
  return true;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return sync && sync->IsSyncActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDetectedEvent(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kGaiaPasswordReuseReporting))
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  auto* const status = specifics->mutable_gaia_password_reuse_event()
                           ->mutable_reuse_detected()
                           ->mutable_status();
  status->set_enabled(IsSafeBrowsingEnabled());

  ExtendedReportingLevel erl = GetExtendedReportingLevel(*GetPrefs());
  switch (erl) {
    case SBER_LEVEL_OFF:
      status->set_safe_browsing_reporting_population(SafeBrowsingStatus::NONE);
      break;
    case SBER_LEVEL_LEGACY:
      status->set_safe_browsing_reporting_population(
          SafeBrowsingStatus::EXTENDED_REPORTING);
      break;
    case SBER_LEVEL_SCOUT:
      status->set_safe_browsing_reporting_population(SafeBrowsingStatus::SCOUT);
      break;
  }
  user_event_service->RecordUserEvent(std::move(specifics));
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

std::unique_ptr<UserEventSpecifics>
ChromePasswordProtectionService::GetUserEventSpecifics(
    content::WebContents* web_contents) {
  content::NavigationEntry* navigation =
      web_contents->GetController().GetLastCommittedEntry();
  if (!navigation)
    return nullptr;

  auto specifics = base::MakeUnique<UserEventSpecifics>();
  specifics->set_event_time_usec(
      GetMicrosecondsSinceWindowsEpoch(base::Time::Now()));
  specifics->set_navigation_id(
      GetMicrosecondsSinceWindowsEpoch(navigation->GetTimestamp()));
  return specifics;
}

void ChromePasswordProtectionService::LogPasswordReuseLookupResult(
    content::WebContents* web_contents,
    PasswordReuseLookup::LookupResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  auto* const reuse_lookup =
      specifics->mutable_gaia_password_reuse_event()->mutable_reuse_lookup();
  reuse_lookup->set_lookup_result(result);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::LogPasswordReuseLookupResultWithVerdict(
    content::WebContents* web_contents,
    PasswordReuseLookup::LookupResult result,
    PasswordReuseLookup::ReputationVerdict verdict,
    const std::string& verdict_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  PasswordReuseLookup* const reuse_lookup =
      specifics->mutable_gaia_password_reuse_event()->mutable_reuse_lookup();
  reuse_lookup->set_lookup_result(result);
  reuse_lookup->set_verdict(verdict);
  reuse_lookup->set_verdict_token(verdict_token);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupEvent(
    content::WebContents* web_contents,
    PasswordProtectionService::RequestOutcome outcome,
    const LoginReputationClientResponse* response) {
  if (!base::FeatureList::IsEnabled(kGaiaPasswordReuseReporting))
    return;

  switch (outcome) {
    case PasswordProtectionService::MATCHED_WHITELIST:
      LogPasswordReuseLookupResult(web_contents,
                                   PasswordReuseLookup::WHITELIST_HIT);
      break;
    case PasswordProtectionService::RESPONSE_ALREADY_CACHED:
      LogPasswordReuseLookupResultWithVerdict(
          web_contents, PasswordReuseLookup::CACHE_HIT,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case PasswordProtectionService::SUCCEEDED:
      LogPasswordReuseLookupResultWithVerdict(
          web_contents, PasswordReuseLookup::REQUEST_SUCCESS,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case PasswordProtectionService::URL_NOT_VALID_FOR_REPUTATION_COMPUTING:
      LogPasswordReuseLookupResult(web_contents,
                                   PasswordReuseLookup::URL_UNSUPPORTED);
      break;
    case PasswordProtectionService::CANCELED:
    case PasswordProtectionService::TIMEDOUT:
    case PasswordProtectionService::DISABLED_DUE_TO_INCOGNITO:
    case PasswordProtectionService::REQUEST_MALFORMED:
    case PasswordProtectionService::FETCH_FAILED:
    case PasswordProtectionService::RESPONSE_MALFORMED:
    case PasswordProtectionService::SERVICE_DESTROYED:
    case PasswordProtectionService::DISABLED_DUE_TO_FEATURE_DISABLED:
    case PasswordProtectionService::DISABLED_DUE_TO_USER_POPULATION:
    case PasswordProtectionService::MAX_OUTCOME:
      LogPasswordReuseLookupResult(web_contents,
                                   PasswordReuseLookup::REQUEST_FAILURE);
      break;
    case PasswordProtectionService::UNKNOWN:
    case PasswordProtectionService::DEPRECATED_NO_EXTENDED_REPORTING:
      NOTREACHED() << __FUNCTION__ << ": outcome: " << outcome;
      break;
  }
}

void ChromePasswordProtectionService::UpdateSecurityState(
    SBThreatType threat_type,
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

  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
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
