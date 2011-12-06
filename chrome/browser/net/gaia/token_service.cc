// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/gaia/token_service.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

// Unfortunately kNumServices must be defined in the .h.
// TODO(chron): Sync doesn't use the TalkToken anymore so we can stop
//              requesting it.
const char* TokenService::kServices[] = {
  GaiaConstants::kGaiaService,
  GaiaConstants::kSyncService,
  GaiaConstants::kTalkService,
  GaiaConstants::kDeviceManagementService,
  GaiaConstants::kLSOService,
};

const char* kUnusedServiceScope = "unused-service-scope";

// Unfortunately kNumOAuthServices must be defined in the .h.
// For OAuth, Chrome uses the OAuth2 service scope as the service name.
const char* TokenService::kOAuthServices[] = {
  GaiaConstants::kSyncServiceOAuth,
};

TokenService::TokenService()
    : profile_(NULL),
      token_loading_query_(0) {
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
  DCHECK(!profile_);
  profile_ = profile;
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
                 chrome::NOTIFICATION_TOKEN_UPDATED,
                 content::Source<Profile>(profile));
}

void TokenService::ResetCredentialsInMemory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Terminate any running fetchers. Callbacks will not return.
  for (int i = 0; i < kNumServices; ++i) {
    fetchers_[i].reset();
  }
  for (int i = 0; i < kNumOAuthServices; ++i) {
    oauth_fetchers_[i].reset();
  }

  // Cancel pending loads. Callbacks will not return.
  if (token_loading_query_) {
    web_data_service_->CancelRequest(token_loading_query_);
    token_loading_query_ = 0;
  }

  token_map_.clear();
  credentials_ = GaiaAuthConsumer::ClientLoginResult();
  oauth_token_.clear();
  oauth_secret_.clear();
}

void TokenService::UpdateCredentials(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  credentials_ = credentials;

  SaveAuthTokenToDB(GaiaConstants::kGaiaLsid, credentials.lsid);
  SaveAuthTokenToDB(GaiaConstants::kGaiaSid, credentials.sid);

  // Cancel any currently running requests.
  for (int i = 0; i < kNumServices; i++) {
    fetchers_[i].reset();
  }
}

void TokenService::UpdateOAuthCredentials(
    const std::string& oauth_token,
    const std::string& oauth_secret) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  oauth_token_ = oauth_token;
  oauth_secret_ = oauth_secret;

  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuthToken, oauth_token);
  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuthSecret, oauth_secret);

  // Cancel any currently running requests.
  for (int i = 0; i < kNumOAuthServices; i++) {
    oauth_fetchers_[i].reset();
  }
}

void TokenService::LoadTokensFromDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (web_data_service_.get())
    token_loading_query_ = web_data_service_->GetAllTokens(this);
}

void TokenService::SaveAuthTokenToDB(const std::string& service,
                                     const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (web_data_service_.get())
    web_data_service_->SetTokenForService(service, auth_token);
}

void TokenService::EraseTokensFromDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (web_data_service_.get())
    web_data_service_->RemoveAllTokens();
}

// static
int TokenService::GetServiceIndex(const std::string& service) {
  for (int i = 0; i < kNumServices; ++i) {
    if (kServices[i] == service)
      return i;
  }
  return -1;
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

bool TokenService::HasOAuthCredentials() const {
  return !oauth_token_.empty() && !oauth_secret_.empty();
}

const std::string& TokenService::GetOAuthToken() const {
  return oauth_token_;
}

const std::string& TokenService::GetOAuthSecret() const {
  return oauth_secret_;
}

void TokenService::StartFetchingTokens() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(AreCredentialsValid());
  for (int i = 0; i < kNumServices; i++) {
    fetchers_[i].reset(new GaiaAuthFetcher(this, source_, getter_));
    fetchers_[i]->StartIssueAuthToken(credentials_.sid,
                                      credentials_.lsid,
                                      kServices[i]);
  }
}

void TokenService::StartFetchingOAuthTokens() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(HasOAuthCredentials());
  for (int i = 0; i < kNumOAuthServices; i++) {
    oauth_fetchers_[i].reset(
        new GaiaOAuthFetcher(this, getter_, profile_, kUnusedServiceScope));
    oauth_fetchers_[i]->StartOAuthWrapBridge(oauth_token_,
                                             oauth_secret_,
                                             GaiaConstants::kGaiaOAuthDuration,
                                             kOAuthServices[i]);
  }
}

// Services dependent on a token will check if a token is available.
// If it isn't, they'll go to sleep until they get a token event.
bool TokenService::HasTokenForService(const char* service) const {
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

bool TokenService::HasOAuthLoginToken() const {
  return HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken);
}

const std::string& TokenService::GetOAuth2LoginRefreshToken() const {
  return GetTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken);
}

const std::string& TokenService::GetOAuth2LoginAccessToken() const {
  return GetTokenForService(GaiaConstants::kGaiaOAuth2LoginAccessToken);
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<TokenService>(this),
      content::Details<const TokenAvailableDetails>(&details));
}

void TokenService::FireTokenRequestFailedNotification(
    const std::string& service,
    const GoogleServiceAuthError& error) {

  TokenRequestFailedDetails details(service, error);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
      content::Source<TokenService>(this),
      content::Details<const TokenRequestFailedDetails>(&details));
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
  // If we got ClientLogin token for "lso" service, then start fetching OAuth2
  // login scoped token pair.
  if (service == GaiaConstants::kLSOService) {
    int index = GetServiceIndex(service);
    DCHECK_NE(-1, index);
    fetchers_[index]->StartOAuthLoginTokenFetch(auth_token);
  }
}

void TokenService::OnIssueAuthTokenFailure(const std::string& service,
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Auth token issuing failed for service:" << service;
  FireTokenRequestFailedNotification(service, error);
}

void TokenService::OnOAuthLoginTokenSuccess(const std::string& refresh_token,
                                            const std::string& access_token,
                                            int expires_in_secs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Got OAuth2 login token pair";
  token_map_[GaiaConstants::kGaiaOAuth2LoginRefreshToken] = refresh_token;
  token_map_[GaiaConstants::kGaiaOAuth2LoginAccessToken] = access_token;
  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
      refresh_token);
  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuth2LoginAccessToken,
      access_token);
  // We don't save expiration information for now.

  FireTokenAvailableNotification(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
      refresh_token);
}

void TokenService::OnOAuthLoginTokenFailure(
   const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "OAuth2 login token pair fetch failed:";
  FireTokenRequestFailedNotification(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, error);
}

void TokenService::OnOAuthGetAccessTokenSuccess(const std::string& token,
                                                const std::string& secret) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "TokenService::OnOAuthGetAccessTokenSuccess";
  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuthToken, token);
  SaveAuthTokenToDB(GaiaConstants::kGaiaOAuthSecret, secret);
  UpdateOAuthCredentials(token, secret);
}

void TokenService::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "TokenService::OnOAuthGetAccessTokenFailure";
}

void TokenService::OnOAuthWrapBridgeSuccess(const std::string& service_scope,
                                            const std::string& token,
                                            const std::string& expires_in) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Got an access token for " << service_scope;
  token_map_[service_scope] = token;
  FireTokenAvailableNotification(service_scope, token);
  SaveAuthTokenToDB(service_scope, token);
}

void TokenService::OnOAuthWrapBridgeFailure(
    const std::string& service_scope,
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Auth token issuing failed for service:" << service_scope;
  FireTokenRequestFailedNotification(service_scope, error);
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

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
      content::Source<TokenService>(this),
      content::NotificationService::NoDetails());
}

// Load tokens from the db_token map into the in memory token map.
void TokenService::LoadTokensIntoMemory(
    const std::map<std::string, std::string>& db_tokens,
    std::map<std::string, std::string>* in_memory_tokens) {

  for (int i = 0; i < kNumServices; i++) {
    LoadSingleTokenIntoMemory(db_tokens, in_memory_tokens, kServices[i]);
  }
  LoadSingleTokenIntoMemory(db_tokens, in_memory_tokens,
      GaiaConstants::kGaiaOAuth2LoginRefreshToken);
  LoadSingleTokenIntoMemory(db_tokens, in_memory_tokens,
      GaiaConstants::kGaiaOAuth2LoginAccessToken);

  if (credentials_.lsid.empty() && credentials_.sid.empty()) {
    // Look for GAIA SID and LSID tokens.  If we have both, and the current
    // crendentials are empty, update the credentials.
    std::string lsid;
    std::string sid;

    if (db_tokens.count(GaiaConstants::kGaiaLsid) > 0)
      lsid = db_tokens.find(GaiaConstants::kGaiaLsid)->second;

    if (db_tokens.count(GaiaConstants::kGaiaSid) > 0)
      sid = db_tokens.find(GaiaConstants::kGaiaSid)->second;

    if (!lsid.empty() && !sid.empty()) {
      UpdateCredentials(GaiaAuthConsumer::ClientLoginResult(sid,
                                                            lsid,
                                                            std::string(),
                                                            std::string()));
    }
  }

  for (int i = 0; i < kNumOAuthServices; i++) {
    LoadSingleTokenIntoMemory(db_tokens, in_memory_tokens, kOAuthServices[i]);
  }

  if (oauth_token_.empty() && oauth_secret_.empty()) {
    // Look for GAIA OAuth1 access token and secret.  If we have both, and the
    // current crendentials are empty, update the credentials.
    std::string oauth_token;
    std::string oauth_secret;

    if (db_tokens.count(GaiaConstants::kGaiaOAuthToken) > 0)
      oauth_token = db_tokens.find(GaiaConstants::kGaiaOAuthToken)->second;

    if (db_tokens.count(GaiaConstants::kGaiaOAuthSecret) > 0)
      oauth_secret = db_tokens.find(GaiaConstants::kGaiaOAuthSecret)->second;

    if (!oauth_token.empty() && !oauth_secret.empty()) {
      UpdateOAuthCredentials(oauth_token, oauth_secret);
    }
  }
}

void TokenService::LoadSingleTokenIntoMemory(
    const std::map<std::string, std::string>& db_tokens,
    std::map<std::string, std::string>* in_memory_tokens,
    const std::string& service) {
  // OnIssueAuthTokenSuccess should come from the same thread.
  // If a token is already present in the map, it could only have
  // come from a DB read or from IssueAuthToken. Since we should never
  // fetch from the DB twice in a browser session, it must be from
  // OnIssueAuthTokenSuccess, which is a live fetcher.
  //
  // Network fetched tokens take priority over DB tokens, so exclude tokens
  // which have already been loaded by the fetcher.
  if (!in_memory_tokens->count(service) && db_tokens.count(service)) {
    std::string db_token = db_tokens.find(service)->second;
    if (!db_token.empty()) {
      VLOG(1) << "Loading " << service << " token from DB: " << db_token;
      (*in_memory_tokens)[service] = db_token;
      FireTokenAvailableNotification(service, db_token);
      // Failures are only for network errors.
    }
  }
}

void TokenService::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOKEN_UPDATED);
  TokenAvailableDetails* tok_details =
      content::Details<TokenAvailableDetails>(details).ptr();
  OnIssueAuthTokenSuccess(tok_details->service(), tok_details->token());
}
