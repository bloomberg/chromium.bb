// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/google/core/browser/google_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/url_util.h"

using content::BrowserThread;
using sync_pb::UserEventSpecifics;
using GaiaPasswordReuse = UserEventSpecifics::GaiaPasswordReuse;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
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

// Given a |web_contents|, returns the navigation id of its last committed
// navigation.
int64_t GetLastCommittedNavigationID(const content::WebContents* web_contents) {
  content::NavigationEntry* navigation =
      web_contents->GetController().GetLastCommittedEntry();
  return navigation
             ? GetMicrosecondsSinceWindowsEpoch(navigation->GetTimestamp())
             : 0;
}

// Opens a URL in a new foreground tab.
void OpenUrlInNewTab(const GURL& url, content::WebContents* web_contents) {
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  web_contents->OpenURL(params);
}

int64_t GetFirstNavIdOrZero(PrefService* prefs) {
  const base::DictionaryValue* unhandled_sync_password_reuses =
      prefs->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  if (!unhandled_sync_password_reuses ||
      unhandled_sync_password_reuses->empty()) {
    return 0;
  }
  base::DictionaryValue::Iterator itr(*unhandled_sync_password_reuses);
  int64_t navigation_id;
  return base::StringToInt64(itr.value().GetString(), &navigation_id)
             ? navigation_id
             : 0;
}

int64_t GetNavigationIDFromPrefsByOrigin(PrefService* prefs,
                                         const Origin& origin) {
  const base::DictionaryValue* unhandled_sync_password_reuses =
      prefs->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  if (!unhandled_sync_password_reuses)
    return 0;

  const base::Value* navigation_id_value =
      unhandled_sync_password_reuses->FindKey(origin.Serialize());

  int64_t navigation_id;
  return navigation_id_value &&
                 base::StringToInt64(navigation_id_value->GetString(),
                                     &navigation_id)
             ? navigation_id
             : 0;
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
      trigger_manager_(sb_service->trigger_manager()),
      profile_(profile),
      navigation_observer_manager_(sb_service->navigation_observer_manager()),
      pref_change_registrar_(new PrefChangeRegistrar) {
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      password_manager::prefs::kSyncPasswordHash,
      base::Bind(&ChromePasswordProtectionService::CheckGaiaPasswordChange,
                 base::Unretained(this)));
  gaia_password_hash_ = profile_->GetPrefs()->GetString(
      password_manager::prefs::kSyncPasswordHash);
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

  if (pref_change_registrar_)
    pref_change_registrar_->RemoveAll();
}

// static
ChromePasswordProtectionService*
ChromePasswordProtectionService::GetPasswordProtectionService(
    Profile* profile) {
  if (g_browser_process && g_browser_process->safe_browsing_service()) {
    return static_cast<safe_browsing::ChromePasswordProtectionService*>(
        g_browser_process->safe_browsing_service()
            ->GetPasswordProtectionService(profile));
  }
  return nullptr;
}

// static
bool ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
    Profile* profile) {
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  if (!service)
    return false;
  auto* unhandled_sync_password_reuses = profile->GetPrefs()->GetDictionary(
      prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  return unhandled_sync_password_reuses &&
         !unhandled_sync_password_reuses->empty();
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
    const std::string& verdict_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't show warning again if there is already a modal warning showing.
  if (IsModalWarningShowingInWebContents(web_contents))
    return;

  // Exit fullscreen if this |web_contents| is showing in fullscreen mode.
  if (web_contents->IsFullscreenForCurrentTab())
    web_contents->ExitFullscreen(true);

  UpdateSecurityState(SB_THREAT_TYPE_PASSWORD_REUSE, web_contents);
  ShowPasswordReuseModalWarningDialog(
      web_contents, this,
      base::BindOnce(&ChromePasswordProtectionService::OnUserAction,
                     base::Unretained(this), web_contents,
                     PasswordProtectionService::MODAL_DIALOG));
  RecordWarningAction(PasswordProtectionService::MODAL_DIALOG,
                      PasswordProtectionService::SHOWN);

  // Updates prefs.
  GURL trigger_url = web_contents->GetLastCommittedURL();
  DCHECK(trigger_url.is_valid());
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  // Since base::Value doesn't support int64_t type, we convert the navigation
  // ID to string format and store it in the preference dictionary.
  update->SetKey(
      Origin::Create(web_contents->GetLastCommittedURL()).Serialize(),
      base::Value(
          base::Int64ToString(GetLastCommittedNavigationID(web_contents))));

  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token);
}

// TODO(jialiul): Handle user actions in separate functions.
void ChromePasswordProtectionService::OnUserAction(
    content::WebContents* web_contents,
    PasswordProtectionService::WarningUIType ui_type,
    PasswordProtectionService::WarningAction action) {
  RecordWarningAction(ui_type, action);
  switch (ui_type) {
    case PasswordProtectionService::PAGE_INFO:
      HandleUserActionOnPageInfo(web_contents, action);
      break;
    case PasswordProtectionService::MODAL_DIALOG:
      HandleUserActionOnModalWarning(web_contents, action);
      break;
    case PasswordProtectionService::CHROME_SETTINGS:
      HandleUserActionOnSettings(web_contents, action);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ChromePasswordProtectionService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromePasswordProtectionService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ChromePasswordProtectionService::MaybeStartThreatDetailsCollection(
    content::WebContents* web_contents,
    const std::string& token) {
  // |trigger_manager_| can be null in test.
  if (!trigger_manager_)
    return;

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_PASSWORD_REUSE;
  resource.url = web_contents->GetLastCommittedURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());
  resource.token = token;
  // Ignores the return of |StartCollectingThreatDetails()| here and let
  // TriggerManager decide whether it should start data collection.
  trigger_manager_->StartCollectingThreatDetails(
      safe_browsing::TriggerType::GAIA_PASSWORD_REUSE, web_contents, resource,
      profile_->GetRequestContext(), /*history_service=*/nullptr,
      TriggerManager::GetSBErrorDisplayOptions(*profile_->GetPrefs(),
                                               *web_contents));
}

void ChromePasswordProtectionService::MaybeFinishCollectingThreatDetails(
    content::WebContents* web_contents,
    bool did_proceed) {
  // |trigger_manager_| can be null in test.
  if (!trigger_manager_)
    return;

  // Since we don't keep track the threat details in progress, it is safe to
  // ignore the result of |FinishCollectingThreatDetails()|. TriggerManager will
  // take care of whether report should be sent.
  trigger_manager_->FinishCollectingThreatDetails(
      safe_browsing::TriggerType::GAIA_PASSWORD_REUSE, web_contents,
      base::TimeDelta::FromMilliseconds(0), did_proceed, /*num_visit=*/0,
      TriggerManager::GetSBErrorDisplayOptions(*profile_->GetPrefs(),
                                               *web_contents));
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
    LoginReputationClientRequest::TriggerType trigger_type,
    RequestOutcome* reason) {
  if (!IsSafeBrowsingEnabled())
    return false;

  // Protected password entry pinging is enabled for all users.
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT)
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

void ChromePasswordProtectionService::LogPasswordReuseDialogInteraction(
    int64_t navigation_id,
    PasswordReuseDialogInteraction::InteractionResult interaction_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kGaiaPasswordReuseReporting))
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecificsWithNavigationId(navigation_id);
  if (!specifics)
    return;

  PasswordReuseDialogInteraction* const dialog_interaction =
      specifics->mutable_gaia_password_reuse_event()
          ->mutable_dialog_interaction();
  dialog_interaction->set_interaction_result(interaction_result);
  user_event_service->RecordUserEvent(std::move(specifics));
}

LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() {
  const AccountInfo account_info = GetAccountInfo();
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
ChromePasswordProtectionService::GetUserEventSpecificsWithNavigationId(
    int64_t navigation_id) {
  if (navigation_id <= 0)
    return nullptr;

  auto specifics = std::make_unique<UserEventSpecifics>();
  specifics->set_event_time_usec(
      GetMicrosecondsSinceWindowsEpoch(base::Time::Now()));
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

std::unique_ptr<UserEventSpecifics>
ChromePasswordProtectionService::GetUserEventSpecifics(
    content::WebContents* web_contents) {
  return GetUserEventSpecificsWithNavigationId(
      GetLastCommittedNavigationID(web_contents));
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
  const GURL url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return;

  const GURL url_with_empty_path = url.GetWithEmptyPath();
  if (threat_type == SB_THREAT_TYPE_SAFE) {
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
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
          url_with_empty_path, /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*whitelist_only=*/false, &current_threat_type)) {
    DCHECK_NE(SB_THREAT_TYPE_UNUSED, current_threat_type);
    if (current_threat_type == threat_type)
      return;
    // Resets previous threat type.
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
  }
  ui_manager_->AddToWhitelistUrlSet(url_with_empty_path, web_contents,
                                    /*is_pending=*/true, threat_type);
}

void ChromePasswordProtectionService::
    RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
        bool all_history,
        const history::URLRows& deleted_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(all_history || !deleted_rows.empty());

  DictionaryPrefUpdate unhandled_sync_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  if (all_history) {
    unhandled_sync_password_reuses->Clear();
    return;
  }

  for (const history::URLRow& row : deleted_rows) {
    if (!row.url().SchemeIsHTTPOrHTTPS())
      continue;
    unhandled_sync_password_reuses->RemoveKey(
        Origin::Create(row.url()).Serialize());
  }
}

void ChromePasswordProtectionService::CheckGaiaPasswordChange() {
  std::string new_gaia_password_hash = profile_->GetPrefs()->GetString(
      password_manager::prefs::kSyncPasswordHash);
  if (gaia_password_hash_ != new_gaia_password_hash) {
    gaia_password_hash_ = new_gaia_password_hash;
    OnGaiaPasswordChanged();
  }
}

void ChromePasswordProtectionService::OnGaiaPasswordChanged() {
  DictionaryPrefUpdate unhandled_sync_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  UMA_HISTOGRAM_COUNTS_100(
      "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged",
      unhandled_sync_password_reuses->size());
  unhandled_sync_password_reuses->Clear();
  for (auto& observer : observer_list_)
    observer.OnGaiaPasswordChanged();
}

bool ChromePasswordProtectionService::UserClickedThroughSBInterstitial(
    content::WebContents* web_contents) {
  SBThreatType current_threat_type;
  if (!ui_manager_->IsUrlWhitelistedOrPendingForWebContents(
          web_contents->GetLastCommittedURL().GetWithEmptyPath(),
          /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*whitelist_only=*/true, &current_threat_type)) {
    return false;
  }
  return current_threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         current_threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         current_threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         current_threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         current_threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
}

AccountInfo ChromePasswordProtectionService::GetAccountInfo() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);

  return signin_manager ? signin_manager->GetAuthenticatedAccountInfo()
                        : AccountInfo();
}

GURL ChromePasswordProtectionService::GetChangePasswordURL() {
  const AccountInfo account_info = GetAccountInfo();
  std::string account_email = account_info.email;
  // This page will prompt for re-auth and then will prompt for a new password.
  std::string account_url =
      "https://myaccount.google.com/signinoptions/"
      "password?utm_source=Google&utm_campaign=PhishGuard";
  url::RawCanonOutputT<char> percent_encoded_email;
  url::RawCanonOutputT<char> percent_encoded_account_url;
  url::EncodeURIComponent(account_email.c_str(), account_email.length(),
                          &percent_encoded_email);
  url::EncodeURIComponent(account_url.c_str(), account_url.length(),
                          &percent_encoded_account_url);
  GURL change_password_url = GURL(base::StringPrintf(
      "https://accounts.google.com/"
      "AccountChooser?Email=%s&continue=%s",
      std::string(percent_encoded_email.data(), percent_encoded_email.length())
          .c_str(),
      std::string(percent_encoded_account_url.data(),
                  percent_encoded_account_url.length())
          .c_str()));
  return google_util::AppendGoogleLocaleParam(
      change_password_url, g_browser_process->GetApplicationLocale());
}

void ChromePasswordProtectionService::HandleUserActionOnModalWarning(
    content::WebContents* web_contents,
    PasswordProtectionService::WarningAction action) {
  const Origin origin = Origin::Create(web_contents->GetLastCommittedURL());
  int64_t navigation_id =
      GetNavigationIDFromPrefsByOrigin(profile_->GetPrefs(), origin);
  if (action == PasswordProtectionService::CHANGE_PASSWORD) {
    LogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
    // Opens chrome://settings page in a new tab.
    OpenUrlInNewTab(GURL(chrome::kChromeUISettingsURL), web_contents);
    RecordWarningAction(PasswordProtectionService::CHROME_SETTINGS,
                        PasswordProtectionService::SHOWN);
  } else if (action == PasswordProtectionService::IGNORE_WARNING) {
    // No need to change state.
    LogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_IGNORED);
  } else if (action == PasswordProtectionService::CLOSE) {
    // No need to change state.
    LogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_UI_IGNORED);
  } else {
    NOTREACHED();
  }

  RemoveWarningRequestsByWebContents(web_contents);
  MaybeFinishCollectingThreatDetails(
      web_contents,
      /*did_proceed=*/action == PasswordProtectionService::CHANGE_PASSWORD);
}

void ChromePasswordProtectionService::HandleUserActionOnPageInfo(
    content::WebContents* web_contents,
    PasswordProtectionService::WarningAction action) {
  GURL url = web_contents->GetLastCommittedURL();
  const Origin origin = Origin::Create(url);

  if (action == PasswordProtectionService::CHANGE_PASSWORD) {
    LogPasswordReuseDialogInteraction(
        GetNavigationIDFromPrefsByOrigin(profile_->GetPrefs(), origin),
        PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);

    // Opens chrome://settings page in a new tab.
    OpenUrlInNewTab(GURL(chrome::kChromeUISettingsURL), web_contents);
    RecordWarningAction(PasswordProtectionService::CHROME_SETTINGS,
                        PasswordProtectionService::SHOWN);
    return;
  }

  if (action == PasswordProtectionService::MARK_AS_LEGITIMATE) {
    // TODO(vakh): There's no good enum to report this dialog interaction.
    // This needs to be investigated.

    UpdateSecurityState(SB_THREAT_TYPE_SAFE, web_contents);
    DictionaryPrefUpdate update(
        profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
    update->RemoveKey(origin.Serialize());
    for (auto& observer : observer_list_)
      observer.OnMarkingSiteAsLegitimate(url);
    return;
  }

  NOTREACHED();
}

void ChromePasswordProtectionService::HandleUserActionOnSettings(
    content::WebContents* web_contents,
    PasswordProtectionService::WarningAction action) {
  DCHECK_EQ(PasswordProtectionService::CHANGE_PASSWORD, action);

  // Gets the first navigation_id from kSafeBrowsingUnhandledSyncPasswordReuses.
  // If there's only one unhandled reuse, getting the first is correct.
  // If there are more than one, we have no way to figure out which
  // event the user is responding to, so just pick the first one.
  LogPasswordReuseDialogInteraction(
      GetFirstNavIdOrZero(profile_->GetPrefs()),
      PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
  // Opens https://account.google.com for user to change password.
  OpenUrlInNewTab(GetChangePasswordURL(), web_contents);
  for (auto& observer : observer_list_)
    observer.OnStartingGaiaPasswordChange();
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
      trigger_manager_(nullptr),
      profile_(profile) {}

std::unique_ptr<PasswordProtectionNavigationThrottle>
MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(kGaiaPasswordReuseReporting))
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  // |service| can be null in tests.
  return service ? service->MaybeCreateNavigationThrottle(navigation_handle)
                 : nullptr;
}

}  // namespace safe_browsing
