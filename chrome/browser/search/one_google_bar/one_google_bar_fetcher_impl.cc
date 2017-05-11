// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/signin/core/browser/access_token_fetcher.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

const char kApiUrl[] = "https://onegoogle-pa.googleapis.com/v1/getbar";

const char kApiKeyFormat[] = "?key=%s";

const char kApiScope[] = "https://www.googleapis.com/auth/onegoogle.api";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

const char kResponsePreamble[] = ")]}'";

// This namespace contains helpers to extract SafeHtml-wrapped strings (see
// https://github.com/google/safe-html-types) from the response json. If there
// is ever a C++ version of the SafeHtml types, we should consider using that
// instead of these custom functions.
namespace safe_html {

bool GetImpl(const base::DictionaryValue& dict,
             const std::string& name,
             const std::string& wrapped_field_name,
             std::string* out) {
  const base::DictionaryValue* value = nullptr;
  if (!dict.GetDictionary(name, &value)) {
    out->clear();
    return false;
  }

  if (!value->GetString(wrapped_field_name, out)) {
    out->clear();
    return false;
  }

  return true;
}

bool GetHtml(const base::DictionaryValue& dict,
             const std::string& name,
             std::string* out) {
  return GetImpl(dict, name, "privateDoNotAccessOrElseSafeHtmlWrappedValue",
                 out);
}

bool GetScript(const base::DictionaryValue& dict,
               const std::string& name,
               std::string* out) {
  return GetImpl(dict, name, "privateDoNotAccessOrElseSafeScriptWrappedValue",
                 out);
}

bool GetStyleSheet(const base::DictionaryValue& dict,
                   const std::string& name,
                   std::string* out) {
  return GetImpl(dict, name,
                 "privateDoNotAccessOrElseSafeStyleSheetWrappedValue", out);
}

}  // namespace safe_html

base::Optional<OneGoogleBarData> JsonToOGBData(const base::Value& value) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    DLOG(WARNING) << "Parse error: top-level dictionary not found";
    return base::nullopt;
  }

  const base::DictionaryValue* one_google_bar = nullptr;
  if (!dict->GetDictionary("oneGoogleBar", &one_google_bar)) {
    DLOG(WARNING) << "Parse error: no oneGoogleBar";
    return base::nullopt;
  }

  OneGoogleBarData result;

  if (!safe_html::GetHtml(*one_google_bar, "html", &result.bar_html)) {
    DLOG(WARNING) << "Parse error: no html";
    return base::nullopt;
  }

  const base::DictionaryValue* page_hooks = nullptr;
  if (!one_google_bar->GetDictionary("pageHooks", &page_hooks)) {
    DLOG(WARNING) << "Parse error: no pageHooks";
    return base::nullopt;
  }

  safe_html::GetScript(*page_hooks, "inHeadScript", &result.in_head_script);
  safe_html::GetStyleSheet(*page_hooks, "inHeadStyle", &result.in_head_style);
  safe_html::GetScript(*page_hooks, "afterBarScript", &result.after_bar_script);
  safe_html::GetHtml(*page_hooks, "endOfBodyHtml", &result.end_of_body_html);
  safe_html::GetScript(*page_hooks, "endOfBodyScript",
                       &result.end_of_body_script);

  return result;
}

}  // namespace

class OneGoogleBarFetcherImpl::AuthenticatedURLFetcher
    : public net::URLFetcherDelegate {
 public:
  using FetchDoneCallback = base::OnceCallback<void(const net::URLFetcher*)>;

  AuthenticatedURLFetcher(SigninManagerBase* signin_manager,
                          OAuth2TokenService* token_service,
                          net::URLRequestContextGetter* request_context,
                          const GURL& google_base_url,
                          FetchDoneCallback callback);
  ~AuthenticatedURLFetcher() override = default;

  void Start();

 private:
  GURL GetApiUrl(bool use_oauth) const;
  std::string GetRequestBody() const;
  std::string GetExtraRequestHeaders(const GURL& url,
                                     const std::string& access_token) const;

  void GotAccessToken(const GoogleServiceAuthError& error,
                      const std::string& access_token);

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  SigninManagerBase* const signin_manager_;
  OAuth2TokenService* const token_service_;
  net::URLRequestContextGetter* const request_context_;
  const GURL google_base_url_;

  FetchDoneCallback callback_;

  std::unique_ptr<AccessTokenFetcher> token_fetcher_;

  // The underlying URLFetcher which does the actual fetch.
  std::unique_ptr<net::URLFetcher> url_fetcher_;
};

OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::AuthenticatedURLFetcher(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context,
    const GURL& google_base_url,
    FetchDoneCallback callback)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      request_context_(request_context),
      google_base_url_(google_base_url),
      callback_(std::move(callback)) {}

void OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::Start() {
  if (!signin_manager_->IsAuthenticated()) {
    GotAccessToken(GoogleServiceAuthError::AuthErrorNone(), std::string());
    return;
  }
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kApiScope);
  token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "one_google", signin_manager_, token_service_, scopes,
      base::BindOnce(&AuthenticatedURLFetcher::GotAccessToken,
                     base::Unretained(this)));
}

GURL OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GetApiUrl(
    bool use_oauth) const {
  std::string api_url = base::GetFieldTrialParamValueByFeature(
      features::kOneGoogleBarOnLocalNtp, "one-google-api-url");
  if (api_url.empty()) {
    api_url = kApiUrl;
  }

  // Append the API key only for unauthenticated requests.
  if (!use_oauth) {
    api_url +=
        base::StringPrintf(kApiKeyFormat, google_apis::GetAPIKey().c_str());
  }

  return GURL(api_url);
}

std::string OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GetRequestBody()
    const {
  std::string override_options = base::GetFieldTrialParamValueByFeature(
      features::kOneGoogleBarOnLocalNtp, "one-google-bar-options");
  if (!override_options.empty()) {
    return override_options;
  }

  base::DictionaryValue dict;
  dict.SetInteger("subproduct", 243);
  dict.SetBoolean("enable_multilogin", true);
  dict.SetString("user_agent", GetUserAgent());
  dict.SetString("accept_language", g_browser_process->GetApplicationLocale());
  dict.SetString("original_request_url", google_base_url_.spec());
  auto material_options_dict = base::MakeUnique<base::DictionaryValue>();
  material_options_dict->SetString("page_title", " ");
  material_options_dict->SetBoolean("position_fixed", true);
  material_options_dict->SetBoolean("disable_moving_userpanel_to_menu", true);
  auto styling_options_dict = base::MakeUnique<base::DictionaryValue>();
  auto background_color_dict = base::MakeUnique<base::DictionaryValue>();
  auto alpha_dict = base::MakeUnique<base::DictionaryValue>();
  alpha_dict->SetInteger("value", 0);
  background_color_dict->Set("alpha", std::move(alpha_dict));
  styling_options_dict->Set("background_color",
                            std::move(background_color_dict));
  material_options_dict->Set("styling_options",
                             std::move(styling_options_dict));
  dict.Set("material_options", std::move(material_options_dict));

  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

std::string
OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GetExtraRequestHeaders(
    const GURL& url,
    const std::string& access_token) const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  if (!access_token.empty()) {
    headers.SetHeader("Authorization",
                      base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                         access_token.c_str()));
  }
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  variations::AppendVariationHeaders(url,
                                     /*incognito=*/false, /*uma_enabled=*/false,
                                     /*is_signed_in=*/false, &headers);
  return headers.ToString();
}

void OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GotAccessToken(
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  // Delete the token fetcher after we leave this method.
  std::unique_ptr<AccessTokenFetcher> deleter(std::move(token_fetcher_));

  bool use_oauth = !access_token.empty();
  GURL url = GetApiUrl(use_oauth);
  url_fetcher_ = net::URLFetcher::Create(0, url, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(request_context_);

  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_AUTH_DATA |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->SetUploadData("application/json", GetRequestBody());
  url_fetcher_->SetExtraRequestHeaders(
      GetExtraRequestHeaders(url, access_token));

  url_fetcher_->Start();
}

void OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::move(callback_).Run(source);
}

OneGoogleBarFetcherImpl::OneGoogleBarFetcherImpl(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context,
    GoogleURLTracker* google_url_tracker)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      request_context_(request_context),
      google_url_tracker_(google_url_tracker),
      weak_ptr_factory_(this) {}

OneGoogleBarFetcherImpl::~OneGoogleBarFetcherImpl() = default;

void OneGoogleBarFetcherImpl::Fetch(OneGoogleCallback callback) {
  callbacks_.push_back(std::move(callback));
  IssueRequestIfNoneOngoing();
}

void OneGoogleBarFetcherImpl::IssueRequestIfNoneOngoing() {
  // If there is an ongoing request, let it complete.
  if (pending_request_.get()) {
    return;
  }

  pending_request_ = base::MakeUnique<AuthenticatedURLFetcher>(
      signin_manager_, token_service_, request_context_,
      google_url_tracker_->google_url(),
      base::BindOnce(&OneGoogleBarFetcherImpl::FetchDone,
                     base::Unretained(this)));
  pending_request_->Start();
}

void OneGoogleBarFetcherImpl::FetchDone(const net::URLFetcher* source) {
  // The fetcher will be deleted when the request is handled.
  std::unique_ptr<AuthenticatedURLFetcher> deleter(std::move(pending_request_));

  const net::URLRequestStatus& request_status = source->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DLOG(WARNING) << "Request failed with error: " << request_status.error()
                  << ": " << net::ErrorToString(request_status.error());
    Respond(base::nullopt);
    return;
  }

  const int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "Response code: " << response_code;
    std::string response;
    source->GetResponseAsString(&response);
    DLOG(WARNING) << "Response: " << response;
    Respond(base::nullopt);
    return;
  }

  std::string response;
  bool success = source->GetResponseAsString(&response);
  DCHECK(success);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kResponsePreamble));
  }

  safe_json::SafeJsonParser::Parse(
      response,
      base::Bind(&OneGoogleBarFetcherImpl::JsonParsed,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OneGoogleBarFetcherImpl::JsonParseFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OneGoogleBarFetcherImpl::JsonParsed(std::unique_ptr<base::Value> value) {
  Respond(JsonToOGBData(*value));
}

void OneGoogleBarFetcherImpl::JsonParseFailed(const std::string& message) {
  DLOG(WARNING) << "Parsing JSON failed: " << message;
  Respond(base::nullopt);
}

void OneGoogleBarFetcherImpl::Respond(
    const base::Optional<OneGoogleBarData>& data) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(data);
  }
  callbacks_.clear();
}
