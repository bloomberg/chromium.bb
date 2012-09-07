// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_setup.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using base::StringPrintf;
using content::BrowserThread;
using net::URLFetcher;

namespace extensions {

namespace {

static const char kChannelSetupAuthError[] = "unauthorized";
static const char kChannelSetupInternalError[] = "internal_error";
static const char kChannelSetupCanceledByUser[] = "canceled_by_user";
static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";
static const char kOAuth2IssueTokenURL[] =
    "https://www.googleapis.com/oauth2/v2/IssueToken";
static const char kOAuth2IssueTokenBodyFormat[] =
    "force=true"
    "&response_type=token"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
static const char kOAuth2IssueTokenScope[] =
    "https://www.googleapis.com/auth/chromewebstore.notification";
static const char kCWSChannelServiceURL[] =
    "https://www.googleapis.com/chromewebstore/v1.1/channels/id";

static AppNotifyChannelSetup::InterceptorForTests*
    g_interceptor_for_tests = NULL;

}  // namespace.

// static
void AppNotifyChannelSetup::SetInterceptorForTests(
    AppNotifyChannelSetup::InterceptorForTests* interceptor) {
  // Only one interceptor at a time, please.
  CHECK(g_interceptor_for_tests == NULL);
  g_interceptor_for_tests = interceptor;
}

AppNotifyChannelSetup::AppNotifyChannelSetup(
    Profile* profile,
    const std::string& extension_id,
    const std::string& client_id,
    const GURL& requestor_url,
    int return_route_id,
    int callback_id,
    AppNotifyChannelUI* ui,
    base::WeakPtr<AppNotifyChannelSetup::Delegate> delegate)
    : profile_(profile->GetOriginalProfile()),
      extension_id_(extension_id),
      client_id_(client_id),
      requestor_url_(requestor_url),
      return_route_id_(return_route_id),
      callback_id_(callback_id),
      delegate_(delegate),
      ui_(ui),
      state_(INITIAL),
      oauth2_access_token_failure_(false) {}

AppNotifyChannelSetup::~AppNotifyChannelSetup() {}

void AppNotifyChannelSetup::Start() {
  AddRef();  // Balanced in ReportResult.

  if (g_interceptor_for_tests) {
    std::string channel_id;
    SetupError error;
    g_interceptor_for_tests->DoIntercept(this, &channel_id, &error);
    state_ = error == NONE ? CHANNEL_ID_SETUP_DONE : ERROR_STATE;

    // Use PostTask so the message loop runs before notifying the delegate, like
    // in the real implementation.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppNotifyChannelSetup::ReportResult,
                   base::Unretained(this), channel_id, error));
    return;
  }

  BeginLogin();
}

void AppNotifyChannelSetup::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  oauth2_access_token_ = access_token;
  EndGetAccessToken(true);
}
void AppNotifyChannelSetup::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  EndGetAccessToken(false);
}

void AppNotifyChannelSetup::OnSyncSetupResult(bool enabled) {
  EndLogin(enabled);
}

void AppNotifyChannelSetup::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK(source);
  switch (state_) {
    case RECORD_GRANT_STARTED:
      EndRecordGrant(source);
      break;
    case CHANNEL_ID_SETUP_STARTED:
      EndGetChannelId(source);
      break;
    default:
      CHECK(false) << "Wrong state: " << state_;
      break;
  }
}

// The contents of |body| should be URL-encoded as appropriate.
URLFetcher* AppNotifyChannelSetup::CreateURLFetcher(
    const GURL& url, const std::string& body, const std::string& auth_token) {
  CHECK(url.is_valid());
  URLFetcher::RequestType type =
      body.empty() ? URLFetcher::GET : URLFetcher::POST;
  URLFetcher* fetcher = net::URLFetcher::Create(0, url, type, this);
  fetcher->SetRequestContext(profile_->GetRequestContext());
  // Always set flags to neither send nor save cookies.
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetExtraRequestHeaders(MakeAuthorizationHeader(auth_token));
  if (!body.empty()) {
    fetcher->SetUploadData("application/x-www-form-urlencoded", body);
  }
  return fetcher;
}

bool AppNotifyChannelSetup::ShouldPromptForLogin() const {
  std::string username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  // Prompt for login if either:
  // 1. the user has not logged in at all or
  // 2. if the user is logged in but there is no OAuth2 login token.
  //    The latter happens for users who are already logged in before the
  //    code to generate OAuth2 login token is released.
  // 3. If the OAuth2 login token does not work anymore.
  //    This can happen if the user explicitly revoked access to Google Chrome
  //    from Google Accounts page.
  return username.empty() ||
         !TokenServiceFactory::GetForProfile(profile_)->HasOAuthLoginToken() ||
         oauth2_access_token_failure_;
}

namespace {

enum LoginNeededHistogram {
  LOGIN_NEEDED,
  LOGIN_NOT_NEEDED,
  LOGIN_NEEDED_BOUNDARY
};

enum LoginSuccessHistogram {
  LOGIN_SUCCESS,
  LOGIN_FAILURE,
  LOGIN_SUCCESS_BOUNDARY
};

}  // namespace

void AppNotifyChannelSetup::BeginLogin() {
  CHECK_EQ(INITIAL, state_);
  state_ = LOGIN_STARTED;
  bool login_needed = ShouldPromptForLogin();
  UMA_HISTOGRAM_ENUMERATION("AppNotify.ChannelSetupBegin",
                            login_needed ? LOGIN_NEEDED : LOGIN_NOT_NEEDED,
                            LOGIN_NEEDED_BOUNDARY);
  if (login_needed) {
    ui_->PromptSyncSetup(this);
    // We'll get called back in OnSyncSetupResult
  } else {
    EndLogin(true);
  }
}

void AppNotifyChannelSetup::EndLogin(bool success) {
  CHECK_EQ(LOGIN_STARTED, state_);
  UMA_HISTOGRAM_ENUMERATION("AppNotify.ChannelSetupLoginResult",
                            success ? LOGIN_SUCCESS : LOGIN_FAILURE,
                            LOGIN_SUCCESS_BOUNDARY);
  if (success) {
    state_ = LOGIN_DONE;
    BeginGetAccessToken();
  } else {
    state_ = ERROR_STATE;
    ReportResult("", USER_CANCELLED);
  }
}

void AppNotifyChannelSetup::BeginGetAccessToken() {
  CHECK_EQ(LOGIN_DONE, state_);
  state_ = FETCH_ACCESS_TOKEN_STARTED;

  oauth2_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, profile_->GetRequestContext()));
  std::vector<std::string> scopes;
  scopes.push_back(GaiaUrls::GetInstance()->oauth1_login_scope());
  scopes.push_back(kOAuth2IssueTokenScope);
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  oauth2_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      token_service->GetOAuth2LoginRefreshToken(),
      scopes);
}

void AppNotifyChannelSetup::EndGetAccessToken(bool success) {
  CHECK_EQ(FETCH_ACCESS_TOKEN_STARTED, state_);
  if (success) {
    state_ = FETCH_ACCESS_TOKEN_DONE;
    BeginRecordGrant();
  } else if (!oauth2_access_token_failure_) {
    oauth2_access_token_failure_ = true;
    // If access token generation fails, then it means somehow the
    // OAuth2 login scoped token became invalid. One way this can happen
    // is if a user explicitly revoked access to Google Chrome from
    // Google Accounts page. In such a case, we should try to show the
    // login setup again to the user, but only if we have not already
    // done so once (to avoid infinite loop).
    state_ = INITIAL;
    BeginLogin();
  } else {
    state_ = ERROR_STATE;
    ReportResult("", INTERNAL_ERROR);
  }
}

void AppNotifyChannelSetup::BeginRecordGrant() {
  CHECK_EQ(FETCH_ACCESS_TOKEN_DONE, state_);
  state_ = RECORD_GRANT_STARTED;

  GURL url = GetOAuth2IssueTokenURL();
  std::string body = MakeOAuth2IssueTokenBody(client_id_, extension_id_);

  url_fetcher_.reset(CreateURLFetcher(url, body, oauth2_access_token_));
  url_fetcher_->Start();
}

void AppNotifyChannelSetup::EndRecordGrant(const net::URLFetcher* source) {
  CHECK_EQ(RECORD_GRANT_STARTED, state_);

  net::URLRequestStatus status = source->GetStatus();

  if (status.status() == net::URLRequestStatus::SUCCESS) {
    if (source->GetResponseCode() == net::HTTP_OK) {
      state_ = RECORD_GRANT_DONE;
      BeginGetChannelId();
    } else {
      // Successfully done with HTTP request, but got an explicit error.
      state_ = ERROR_STATE;
      ReportResult("", AUTH_ERROR);
    }
  } else {
    // Could not do HTTP request.
    state_ = ERROR_STATE;
    ReportResult("", INTERNAL_ERROR);
  }
}

void AppNotifyChannelSetup::BeginGetChannelId() {
  CHECK_EQ(RECORD_GRANT_DONE, state_);
  state_ = CHANNEL_ID_SETUP_STARTED;

  GURL url = GetCWSChannelServiceURL();

  url_fetcher_.reset(CreateURLFetcher(url, "", oauth2_access_token_));
  url_fetcher_->Start();
}

void AppNotifyChannelSetup::EndGetChannelId(const net::URLFetcher* source) {
  CHECK_EQ(CHANNEL_ID_SETUP_STARTED, state_);
  net::URLRequestStatus status = source->GetStatus();

  if (status.status() == net::URLRequestStatus::SUCCESS) {
    if (source->GetResponseCode() == net::HTTP_OK) {
      std::string data;
      source->GetResponseAsString(&data);
      std::string channel_id;
      bool result = ParseCWSChannelServiceResponse(data, &channel_id);
      if (result) {
        state_ = CHANNEL_ID_SETUP_DONE;
        ReportResult(channel_id, NONE);
      } else {
        state_ = ERROR_STATE;
        ReportResult("", INTERNAL_ERROR);
      }
    } else {
      // Successfully done with HTTP request, but got an explicit error.
      state_ = ERROR_STATE;
      ReportResult("", AUTH_ERROR);
    }
  } else {
    // Could not do HTTP request.
    state_ = ERROR_STATE;
    ReportResult("", INTERNAL_ERROR);
  }
}

void AppNotifyChannelSetup::ReportResult(
    const std::string& channel_id,
    SetupError error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(state_ == CHANNEL_ID_SETUP_DONE || state_ == ERROR_STATE);

  UMA_HISTOGRAM_ENUMERATION("AppNotification.ChannelSetupFinalResult",
                            error, SETUP_ERROR_BOUNDARY);
  if (delegate_.get()) {
    delegate_->AppNotifyChannelSetupComplete(
        channel_id, GetErrorString(error), this);
  }
  Release(); // Matches AddRef in Start.
}

// static
std::string AppNotifyChannelSetup::GetErrorString(SetupError error) {
  switch (error) {
    case NONE:           return "";
    case AUTH_ERROR:     return kChannelSetupAuthError;
    case INTERNAL_ERROR: return kChannelSetupInternalError;
    case USER_CANCELLED: return kChannelSetupCanceledByUser;
    case SETUP_ERROR_BOUNDARY: {
      CHECK(false);
      break;
    }
  }
  CHECK(false) << "Unhandled enum value";
  return std::string();
}


// static
GURL AppNotifyChannelSetup::GetCWSChannelServiceURL() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppNotifyChannelServerURL)) {
    std::string switch_value = command_line->GetSwitchValueASCII(
        switches::kAppNotifyChannelServerURL);
    GURL result(switch_value);
    if (result.is_valid()) {
      return result;
    } else {
      LOG(ERROR) << "Invalid value for " <<
          switches::kAppNotifyChannelServerURL;
    }
  }
  return GURL(kCWSChannelServiceURL);
}

// static
GURL AppNotifyChannelSetup::GetOAuth2IssueTokenURL() {
  return GURL(kOAuth2IssueTokenURL);
}

// static
std::string AppNotifyChannelSetup::MakeOAuth2IssueTokenBody(
    const std::string& oauth_client_id,
    const std::string& extension_id) {
  return StringPrintf(kOAuth2IssueTokenBodyFormat,
      kOAuth2IssueTokenScope,
      net::EscapeUrlEncodedData(oauth_client_id, true).c_str(),
      net::EscapeUrlEncodedData(extension_id, true).c_str());
}

// static
std::string AppNotifyChannelSetup::MakeAuthorizationHeader(
    const std::string& auth_token) {
  return StringPrintf(kAuthorizationHeaderFormat, auth_token.c_str());
}

// static
bool AppNotifyChannelSetup::ParseCWSChannelServiceResponse(
    const std::string& data, std::string* result) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  return dict->GetString("id", result);
}

}  // namespace extensions
