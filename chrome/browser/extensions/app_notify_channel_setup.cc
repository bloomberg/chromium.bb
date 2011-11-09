// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_setup.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/stringprintf.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_status.h"

using base::StringPrintf;
using content::BrowserThread;
using content::URLFetcher;

namespace {
static const char kChannelSetupAuthError[] = "unauthorized";
static const char kChannelSetupInternalError[] = "internal_error";
static const char kChannelSetupCanceledByUser[] = "canceled_by_user";
// TODO(munjal): Change these to use production URLs when we have them.
static const char kAuthorizationHeaderFormat[] =
    "Authorization: GoogleLogin auth=%s";
static const char kOAuth2IssueTokenURL[] =
    "https://www-googleapis-staging.sandbox.google.com/oauth2/v2/IssueToken";
static const char kOAuth2IssueTokenBodyFormat[] =
    "force=true"
    "&response_type=token"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
static const char kOAuth2IssueTokenScope[] =
    "https://www.googleapis.com/auth/chromewebstore.notification";
static const char kCWSChannelServiceURL[] =
    "https://www-googleapis-staging.sandbox.google.com/chromewebstore/"
    "v1internal/channels/app_id/oauth_client_id";
}  // namespace.

AppNotifyChannelSetup::AppNotifyChannelSetup(
    Profile* profile,
    const std::string& extension_id,
    const std::string& client_id,
    const GURL& requestor_url,
    int return_route_id,
    int callback_id,
    AppNotifyChannelUI* ui,
    base::WeakPtr<AppNotifyChannelSetup::Delegate> delegate)
    : profile_(profile),
      extension_id_(extension_id),
      client_id_(client_id),
      requestor_url_(requestor_url),
      return_route_id_(return_route_id),
      callback_id_(callback_id),
      delegate_(delegate),
      ui_(ui),
      state_(INITIAL) {}

AppNotifyChannelSetup::~AppNotifyChannelSetup() {}

void AppNotifyChannelSetup::Start() {
  AddRef(); // Balanced in ReportResult.

  CHECK_EQ(INITIAL, state_);

  // Check if the user is logged in to the browser.
  std::string username = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  if (username.empty()) {
    state_ = LOGIN_STARTED;
    ui_->PromptSyncSetup(this);
    return; // We'll get called back in OnSyncSetupResult
  }

  state_ = LOGIN_DONE;
  BeginRecordGrant();
}

void AppNotifyChannelSetup::OnSyncSetupResult(bool enabled) {
  CHECK_EQ(LOGIN_STARTED, state_);
  if (enabled) {
    state_ = LOGIN_DONE;
    BeginRecordGrant();
  } else {
    state_ = ERROR_STATE;
    ReportResult("", kChannelSetupCanceledByUser);
  }
}

void AppNotifyChannelSetup::OnURLFetchComplete(const URLFetcher* source) {
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
  URLFetcher* fetcher = URLFetcher::Create(0, url, type, this);
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

void AppNotifyChannelSetup::BeginRecordGrant() {
  CHECK_EQ(LOGIN_DONE, state_);
  state_ = RECORD_GRANT_STARTED;

  GURL url = GetOAuth2IssueTokenURL();
  std::string body = MakeOAuth2IssueTokenBody(client_id_, extension_id_);

  url_fetcher_.reset(CreateURLFetcher(url, body, GetLSOAuthToken()));
  url_fetcher_->Start();
}

void AppNotifyChannelSetup::EndRecordGrant(const URLFetcher* source) {
  CHECK_EQ(RECORD_GRANT_STARTED, state_);

  net::URLRequestStatus status = source->GetStatus();

  if (status.status() == net::URLRequestStatus::SUCCESS) {
    if (source->GetResponseCode() == RC_REQUEST_OK) {
      state_ = RECORD_GRANT_DONE;
      BeginGetChannelId();
    } else {
      // Successfully done with HTTP request, but got an explicit error.
      state_ = ERROR_STATE;
      ReportResult("", kChannelSetupAuthError);
    }
  } else {
    // Could not do HTTP request.
    state_ = ERROR_STATE;
    ReportResult("", kChannelSetupInternalError);
  }
}

void AppNotifyChannelSetup::BeginGetChannelId() {
  CHECK_EQ(RECORD_GRANT_DONE, state_);
  state_ = CHANNEL_ID_SETUP_STARTED;

  GURL url = GetCWSChannelServiceURL();

  url_fetcher_.reset(CreateURLFetcher(url, "", GetCWSAuthToken()));
  url_fetcher_->Start();
}

void AppNotifyChannelSetup::EndGetChannelId(const URLFetcher* source) {
  CHECK_EQ(CHANNEL_ID_SETUP_STARTED, state_);
  net::URLRequestStatus status = source->GetStatus();

  if (status.status() == net::URLRequestStatus::SUCCESS) {
    if (source->GetResponseCode() == RC_REQUEST_OK) {
      std::string data;
      source->GetResponseAsString(&data);
      std::string channel_id;
      bool result = ParseCWSChannelServiceResponse(data, &channel_id);
      if (result) {
        state_ = CHANNEL_ID_SETUP_DONE;
        ReportResult(channel_id, "");
      } else {
        state_ = ERROR_STATE;
        ReportResult("", kChannelSetupInternalError);
      }
    } else {
      // Successfully done with HTTP request, but got an explicit error.
      state_ = ERROR_STATE;
      ReportResult("", kChannelSetupAuthError);
    }
  } else {
    // Could not do HTTP request.
    state_ = ERROR_STATE;
    ReportResult("", kChannelSetupInternalError);
  }
}

void AppNotifyChannelSetup::ReportResult(
    const std::string& channel_id,
    const std::string& error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(state_ == CHANNEL_ID_SETUP_DONE || state_ == ERROR_STATE);

  if (delegate_.get()) {
    delegate_->AppNotifyChannelSetupComplete(
        channel_id, error, return_route_id_, callback_id_);
  }
  Release(); // Matches AddRef in Start.
}

// static
GURL AppNotifyChannelSetup::GetCWSChannelServiceURL() {
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

std::string AppNotifyChannelSetup::GetLSOAuthToken() {
  return profile_->GetTokenService()->GetTokenForService(
      GaiaConstants::kLSOService);
}

std::string AppNotifyChannelSetup::GetCWSAuthToken() {
  return profile_->GetTokenService()->GetTokenForService(
      GaiaConstants::kCWSService);
}

// static
bool AppNotifyChannelSetup::ParseCWSChannelServiceResponse(
    const std::string& data, std::string* result) {
  base::JSONReader reader;
  scoped_ptr<base::Value> value(reader.Read(data, false));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  Value* channel_id_value;
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  if (!dict->Get("id", &channel_id_value))
    return false;
  if (channel_id_value->GetType() != base::Value::TYPE_STRING)
    return false;

  StringValue* channel_id = static_cast<StringValue*>(channel_id_value);
  channel_id->GetAsString(result);
  return true;
}
