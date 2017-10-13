// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/common/chrome_content_client.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

namespace {

const char kApiPath[] = "/async/newtab_ogb";

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
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_html_wrapped_value", out);
}

bool GetScript(const base::DictionaryValue& dict,
               const std::string& name,
               std::string* out) {
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_script_wrapped_value",
                 out);
}

bool GetStyleSheet(const base::DictionaryValue& dict,
                   const std::string& name,
                   std::string* out) {
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_style_sheet_wrapped_value",
                 out);
}

}  // namespace safe_html

base::Optional<OneGoogleBarData> JsonToOGBData(const base::Value& value) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    DLOG(WARNING) << "Parse error: top-level dictionary not found";
    return base::nullopt;
  }

  const base::DictionaryValue* update = nullptr;
  if (!dict->GetDictionary("update", &update)) {
    DLOG(WARNING) << "Parse error: no update";
    return base::nullopt;
  }

  const base::DictionaryValue* one_google_bar = nullptr;
  if (!update->GetDictionary("ogb", &one_google_bar)) {
    DLOG(WARNING) << "Parse error: no ogb";
    return base::nullopt;
  }

  OneGoogleBarData result;

  if (!safe_html::GetHtml(*one_google_bar, "html", &result.bar_html)) {
    DLOG(WARNING) << "Parse error: no html";
    return base::nullopt;
  }

  const base::DictionaryValue* page_hooks = nullptr;
  if (!one_google_bar->GetDictionary("page_hooks", &page_hooks)) {
    DLOG(WARNING) << "Parse error: no page_hooks";
    return base::nullopt;
  }

  safe_html::GetScript(*page_hooks, "in_head_script", &result.in_head_script);
  safe_html::GetStyleSheet(*page_hooks, "in_head_style", &result.in_head_style);
  safe_html::GetScript(*page_hooks, "after_bar_script",
                       &result.after_bar_script);
  safe_html::GetHtml(*page_hooks, "end_of_body_html", &result.end_of_body_html);
  safe_html::GetScript(*page_hooks, "end_of_body_script",
                       &result.end_of_body_script);

  return result;
}

}  // namespace

class OneGoogleBarFetcherImpl::AuthenticatedURLFetcher
    : public net::URLFetcherDelegate {
 public:
  using FetchDoneCallback = base::OnceCallback<void(const net::URLFetcher*)>;

  AuthenticatedURLFetcher(net::URLRequestContextGetter* request_context,
                          const GURL& google_base_url,
                          const std::string& application_locale,
                          const base::Optional<std::string>& api_url_override,
                          FetchDoneCallback callback);
  ~AuthenticatedURLFetcher() override = default;

  void Start();

 private:
  GURL GetApiUrl() const;
  std::string GetExtraRequestHeaders(const GURL& url) const;

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* const request_context_;
  const GURL google_base_url_;
  const std::string application_locale_;
  const base::Optional<std::string> api_url_override_;

  FetchDoneCallback callback_;

  // The underlying URLFetcher which does the actual fetch.
  std::unique_ptr<net::URLFetcher> url_fetcher_;
};

OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::AuthenticatedURLFetcher(
    net::URLRequestContextGetter* request_context,
    const GURL& google_base_url,
    const std::string& application_locale,
    const base::Optional<std::string>& api_url_override,
    FetchDoneCallback callback)
    : request_context_(request_context),
      google_base_url_(google_base_url),
      application_locale_(application_locale),
      api_url_override_(api_url_override),
      callback_(std::move(callback)) {}

GURL OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GetApiUrl() const {
  GURL api_url = google_base_url_.Resolve(api_url_override_.value_or(kApiPath));

  // Add the "hl=" parameter.
  return net::AppendQueryParameter(api_url, "hl", application_locale_);
}

std::string
OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::GetExtraRequestHeaders(
    const GURL& url) const {
  net::HttpRequestHeaders headers;
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  variations::AppendVariationHeaders(url,
                                     /*incognito=*/false, /*uma_enabled=*/false,
                                     /*is_signed_in=*/false, &headers);
  return headers.ToString();
}

void OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::Start() {
  GURL url = GetApiUrl();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("one_google_bar_service", R"(
        semantics {
          sender: "One Google Bar Service"
          description: "Downloads the 'One Google' bar."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider."
          data: "Credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  url_fetcher_ = net::URLFetcher::Create(0, url, net::URLFetcher::GET, this,
                                         traffic_annotation);
  url_fetcher_->SetRequestContext(request_context_);

  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->SetExtraRequestHeaders(GetExtraRequestHeaders(url));

  url_fetcher_->Start();
}

void OneGoogleBarFetcherImpl::AuthenticatedURLFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::move(callback_).Run(source);
}

OneGoogleBarFetcherImpl::OneGoogleBarFetcherImpl(
    net::URLRequestContextGetter* request_context,
    GoogleURLTracker* google_url_tracker,
    const std::string& application_locale,
    const base::Optional<std::string>& api_url_override)
    : request_context_(request_context),
      google_url_tracker_(google_url_tracker),
      application_locale_(application_locale),
      api_url_override_(api_url_override),
      weak_ptr_factory_(this) {}

OneGoogleBarFetcherImpl::~OneGoogleBarFetcherImpl() = default;

void OneGoogleBarFetcherImpl::Fetch(OneGoogleCallback callback) {
  callbacks_.push_back(std::move(callback));

  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = google_url_tracker_->google_url();
  }

  // Note: If there is an ongoing request, abandon it. It's possible that
  // something has changed in the meantime (e.g. signin state) that would make
  // the result obsolete.
  pending_request_ = base::MakeUnique<AuthenticatedURLFetcher>(
      request_context_, google_base_url, application_locale_, api_url_override_,
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
    Respond(Status::TRANSIENT_ERROR, base::nullopt);
    return;
  }

  const int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "Response code: " << response_code;
    std::string response;
    source->GetResponseAsString(&response);
    DLOG(WARNING) << "Response: " << response;
    Respond(Status::FATAL_ERROR, base::nullopt);
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

  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      response,
      base::Bind(&OneGoogleBarFetcherImpl::JsonParsed,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OneGoogleBarFetcherImpl::JsonParseFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OneGoogleBarFetcherImpl::JsonParsed(std::unique_ptr<base::Value> value) {
  base::Optional<OneGoogleBarData> result = JsonToOGBData(*value);
  Respond(result.has_value() ? Status::OK : Status::FATAL_ERROR, result);
}

void OneGoogleBarFetcherImpl::JsonParseFailed(const std::string& message) {
  DLOG(WARNING) << "Parsing JSON failed: " << message;
  Respond(Status::FATAL_ERROR, base::nullopt);
}

void OneGoogleBarFetcherImpl::Respond(
    Status status,
    const base::Optional<OneGoogleBarData>& data) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(status, data);
  }
  callbacks_.clear();
}
