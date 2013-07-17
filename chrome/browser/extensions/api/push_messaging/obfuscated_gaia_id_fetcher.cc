// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/obfuscated_gaia_id_fetcher.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {

// URL of the service to get obfuscated Gaia ID (here misnamed channel ID).
static const char kCWSChannelServiceURL[] =
    "https://www.googleapis.com/gcm_for_chrome/v1/channels/id";

GoogleServiceAuthError CreateAuthError(const URLFetcher* source) {
  if (source->GetStatus().status() == URLRequestStatus::CANCELED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  } else {
    // TODO(munjal): Improve error handling. Currently we return connection
    // error for even application level errors. We need to either expand the
    // GoogleServiceAuthError enum or create a new one to report better
    // errors.
    if (source->GetStatus().is_success()) {
      DLOG(WARNING) << "Remote server returned " << source->GetResponseCode();
      return GoogleServiceAuthError::FromConnectionError(
          source->GetResponseCode());
    } else {
      DLOG(WARNING) << "URLFetcher failed: " << source->GetStatus().error();
      return GoogleServiceAuthError::FromConnectionError(
          source->GetStatus().error());
    }
  }
}

}  // namespace

namespace extensions {

ObfuscatedGaiaIdFetcher::ObfuscatedGaiaIdFetcher(
    URLRequestContextGetter* context,
    Delegate* delegate,
    const std::string& access_token)
    : OAuth2ApiCallFlow(context, std::string(), access_token, GetScopes()),
      delegate_(delegate) {
  DCHECK(delegate);
}

ObfuscatedGaiaIdFetcher::~ObfuscatedGaiaIdFetcher() { }

// Returns a vector of scopes needed to call the API to get obfuscated Gaia ID.
std::vector<std::string> ObfuscatedGaiaIdFetcher::GetScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("https://www.googleapis.com/auth/gcm_for_chrome.readonly");
  return scopes;
}

void ObfuscatedGaiaIdFetcher::ReportSuccess(const std::string& obfuscated_id) {
  if (delegate_)
    delegate_->OnObfuscatedGaiaIdFetchSuccess(obfuscated_id);
}

void ObfuscatedGaiaIdFetcher::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_)
    delegate_->OnObfuscatedGaiaIdFetchFailure(error);
}

GURL ObfuscatedGaiaIdFetcher::CreateApiCallUrl() {
  return GURL(kCWSChannelServiceURL);
}

std::string ObfuscatedGaiaIdFetcher::CreateApiCallBody() {
  // Nothing to do here, we don't need a body for this request, the URL
  // encodes all the proper arguments.
  return std::string();
}

void ObfuscatedGaiaIdFetcher::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  // TODO(munjal): Change error code paths in this method to report an
  // internal error.
  std::string response_body;
  CHECK(source->GetResponseAsString(&response_body));

  std::string obfuscated_id;
  if (ParseResponse(response_body, &obfuscated_id))
    ReportSuccess(obfuscated_id);
  else
    // we picked 101 arbitrarily to help us correlate the error with this code.
    ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
}

void ObfuscatedGaiaIdFetcher::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  ReportFailure(CreateAuthError(source));
}

void ObfuscatedGaiaIdFetcher::ProcessNewAccessToken(
    const std::string& obfuscated_id)  {
  // We generate a new access token every time instead of storing the access
  // token since access tokens expire every hour and we expect to get
  // obfuscated Gaia ID very infrequently.
}

void ObfuscatedGaiaIdFetcher::ProcessMintAccessTokenFailure(
    const GoogleServiceAuthError& error)  {
  // We failed to generate the token needed to call the API to get
  // the obfuscated Gaia ID, so report failure to the caller.
  ReportFailure(error);
}

// static
bool ObfuscatedGaiaIdFetcher::ParseResponse(
    const std::string& data, std::string* result) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));

  if (!value.get())
    return false;

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict))
    return false;

  return dict->GetString("id", result);
}

}  // namespace extensions
