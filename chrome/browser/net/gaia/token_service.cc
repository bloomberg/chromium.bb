// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/gaia/token_service.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "net/url_request/url_request_context_getter.h"

// Unfortunately kNumServices must be defined in the .h.
// TODO(chron): Sync doesn't use the TalkToken anymore so we can stop
//              requesting it.
const char* TokenService::kServices[] = {
  GaiaConstants::kGaiaService,
  GaiaConstants::kSyncService,
  GaiaConstants::kTalkService,
  GaiaConstants::kDeviceManagementService
};

TokenService::TokenService()
    : token_loading_query_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

TokenService::~TokenService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResetCredentialsInMemory();
}

void TokenService::Initialize(const char* const source,
                              Profile* profile) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!source_.empty()) {
    // Already initialized.
    return;
  }
  getter_ = profile->GetRequestContext();
  // Since the user can create a bookmark in incognito, sync may be running.
  // Thus we have to go for explicit access.
  web_data_service_ = profile->GetWebDataService(Profile::EXPLICIT_ACCESS);
  source_ = std::string(source);

#ifndef NDEBUG
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  // Allow the token service to be cleared from the command line.
  if (cmd_line->HasSwitch(switches::kClearTokenService))
    EraseTokensFromDB();

  // Allow a token to be injected from the command line.
  if (cmd_line->HasSwitch(switches::kSetToken)) {
    std::string value = cmd_line->GetSwitchValueASCII(switches::kSetToken);
    int separator = value.find(':');
    std::string service = value.substr(0, separator);
    std::string token = value.substr(separator + 1);
    token_map_[service] = token;
    SaveAuthTokenToDB(service, token);
  }
#endif

  registrar_.Add(this,
                 NotificationType::TOKEN_UPDATED,
                 NotificationService::AllSources());
}

void TokenService::ResetCredentialsInMemory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Terminate any running fetchers. Callbacks will not return.
  for (int i = 0; i < kNumServices; i++) {
    fetchers_[i].reset();
  }

  // Cancel pending loads. Callbacks will not return.
  if (token_loading_query_) {
    web_data_service_->CancelRequest(token_loading_query_);
    token_loading_query_ = 0;
  }

  token_map_.clear();
  credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void TokenService::UpdateCredentials(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  credentials_ = credentials;

  // Cancels any currently running requests.
  for (int i = 0; i < kNumServices; i++) {
    fetchers_[i].reset(new GaiaAuthFetcher(this, source_, getter_));
  }
}

void TokenService::LoadTokensFromDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  token_loading_query_ = web_data_service_->GetAllTokens(this);
}

void TokenService::SaveAuthTokenToDB(const std::string& service,
                                     const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_data_service_->SetTokenForService(service, auth_token);
}

void TokenService::EraseTokensFromDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_data_service_->RemoveAllTokens();
}

bool TokenService::AreCredentialsValid() const {
  return !credentials_.lsid.empty() && !credentials_.sid.empty();
}

bool TokenService::HasLsid() const {
  return !credentials_.lsid.empty();
}

const std::string& TokenService::GetLsid() const {
  return credentials_.lsid;
}

void TokenService::StartFetchingTokens() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(AreCredentialsValid());
  for (int i = 0; i < kNumServices; i++) {
    fetchers_[i]->StartIssueAuthToken(credentials_.sid,
                                      credentials_.lsid,
                                      kServices[i]);
  }
}

// Services dependent on a token will check if a token is available.
// If it isn't, they'll go to sleep until they get a token event.
bool TokenService::HasTokenForService(const char* const service) const {
  return token_map_.count(service) > 0;
}

const std::string& TokenService::GetTokenForService(
    const char* const service) const {

  if (token_map_.count(service) > 0) {
    // Note map[key] is not const.
    return (*token_map_.find(service)).second;
  }
  return EmptyString();
}

// Note that this can fire twice or more for any given service.
// It can fire once from the DB read, and then once from the initial
// fetcher. Future fetches can cause more notification firings.
// The DB read will not however fire a notification if the fetcher
// returned first. So it's always safe to use the latest notification.
void TokenService::FireTokenAvailableNotification(
    const std::string& service,
    const std::string& auth_token) {

  TokenAvailableDetails details(service, auth_token);
  NotificationService::current()->Notify(
      NotificationType::TOKEN_AVAILABLE,
      Source<TokenService>(this),
      Details<const TokenAvailableDetails>(&details));
}

void TokenService::FireTokenRequestFailedNotification(
    const std::string& service,
    const GoogleServiceAuthError& error) {

  TokenRequestFailedDetails details(service, error);
  NotificationService::current()->Notify(
      NotificationType::TOKEN_REQUEST_FAILED,
      Source<TokenService>(this),
      Details<const TokenRequestFailedDetails>(&details));
}

void TokenService::IssueAuthTokenForTest(const std::string& service,
                                         const std::string& auth_token) {
  token_map_[service] = auth_token;
  FireTokenAvailableNotification(service, auth_token);
}

void TokenService::OnIssueAuthTokenSuccess(const std::string& service,
                                           const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Got an authorization token for " << service;
  token_map_[service] = auth_token;
  FireTokenAvailableNotification(service, auth_token);
  SaveAuthTokenToDB(service, auth_token);
}

void TokenService::OnIssueAuthTokenFailure(const std::string& service,
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Auth token issuing failed for service:" << service;
  FireTokenRequestFailedNotification(service, error);
}

void TokenService::OnWebDataServiceRequestDone(WebDataService::Handle h,
                                               const WDTypedResult* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(token_loading_query_);
  token_loading_query_ = 0;

  // If the fetch failed, there will be no result. In that case, we just don't
  // load any tokens at all from the DB.
  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<std::map<std::string, std::string> > * token_result =
        static_cast<const WDResult<std::map<std::string, std::string> > * > (
            result);
    LoadTokensIntoMemory(token_result->GetValue(), &token_map_);
  }

  NotificationService::current()->Notify(
      NotificationType::TOKEN_LOADING_FINISHED,
      Source<TokenService>(this),
      NotificationService::NoDetails());
}

// Load tokens from the db_token map into the in memory token map.
void TokenService::LoadTokensIntoMemory(
    const std::map<std::string, std::string>& db_tokens,
    std::map<std::string, std::string>* in_memory_tokens) {

  for (int i = 0; i < kNumServices; i++) {
    // OnIssueAuthTokenSuccess should come from the same thread.
    // If a token is already present in the map, it could only have
    // come from a DB read or from IssueAuthToken. Since we should never
    // fetch from the DB twice in a browser session, it must be from
    // OnIssueAuthTokenSuccess, which is a live fetcher.
    //
    // Network fetched tokens take priority over DB tokens, so exclude tokens
    // which have already been loaded by the fetcher.
    if (!in_memory_tokens->count(kServices[i]) &&
        db_tokens.count(kServices[i])) {
      std::string db_token = db_tokens.find(kServices[i])->second;
      if (!db_token.empty()) {
        VLOG(1) << "Loading " << kServices[i] << "token from DB: " << db_token;
        (*in_memory_tokens)[kServices[i]] = db_token;
        FireTokenAvailableNotification(kServices[i], db_token);
        // Failures are only for network errors.
      }
    }
  }
}

void TokenService::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TOKEN_UPDATED);
  TokenAvailableDetails* tok_details =
      Details<TokenAvailableDetails>(details).ptr();
  OnIssueAuthTokenSuccess(tok_details->service(), tok_details->token());
}
