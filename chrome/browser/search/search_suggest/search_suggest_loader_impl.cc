// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_loader_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

const char kNewTabSearchSuggestionsApiPath[] = "/async/newtab_suggestions";

const char kSearchSuggestResponsePreamble[] = ")]}'";

base::Optional<SearchSuggestData> JsonToSearchSuggestionData(
    const base::Value& value) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    DVLOG(1) << "Parse error: top-level dictionary not found";
    return base::nullopt;
  }

  const base::DictionaryValue* update = nullptr;
  if (!dict->GetDictionary("update", &update)) {
    DVLOG(1) << "Parse error: no update";
    return base::nullopt;
  }

  const base::DictionaryValue* query_suggestions = nullptr;
  if (!update->GetDictionary("query_suggestions", &query_suggestions)) {
    DVLOG(1) << "Parse error: no query_suggestions";
    return base::nullopt;
  }

  // TODO(crbug.com/919905): Investigate if SafeHtml should be used here.
  std::string suggestions_html = std::string();
  if (!query_suggestions->GetString("query_suggestions_with_html",
                                    &suggestions_html)) {
    DVLOG(1) << "Parse error: no query_suggestions_with_html";
    return base::nullopt;
  }

  SearchSuggestData result;
  result.suggestions_html = suggestions_html;

  std::string end_of_body_script = std::string();
  if (!query_suggestions->GetString("script", &end_of_body_script)) {
    DVLOG(1) << "Parse error: no script";
    return base::nullopt;
  }

  result.end_of_body_script = end_of_body_script;

  int impression_cap_expire_time_ms;
  if (!query_suggestions->GetInteger("impression_cap_expire_time_ms",
                                     &impression_cap_expire_time_ms)) {
    DVLOG(1) << "Parse error: no impression_cap_expire_time_ms";
    return base::nullopt;
  }

  result.impression_cap_expire_time_ms = impression_cap_expire_time_ms;

  int request_freeze_time_ms;
  if (!query_suggestions->GetInteger("request_freeze_time_ms",
                                     &request_freeze_time_ms)) {
    DVLOG(1) << "Parse error: no request_freeze_time_ms";
    return base::nullopt;
  }

  result.request_freeze_time_ms = request_freeze_time_ms;

  int max_impressions;
  if (!query_suggestions->GetInteger("max_impressions", &max_impressions)) {
    DVLOG(1) << "Parse error: no max_impressions";
    return base::nullopt;
  }

  result.max_impressions = max_impressions;

  return result;
}

}  // namespace

class SearchSuggestLoaderImpl::AuthenticatedURLLoader {
 public:
  using LoadDoneCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* simple_loader,
                              std::unique_ptr<std::string> response_body)>;

  AuthenticatedURLLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL api_url,
      LoadDoneCallback callback);
  ~AuthenticatedURLLoader() = default;

  void Start();

 private:
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL api_url_;

  LoadDoneCallback callback_;

  // The underlying SimpleURLLoader which does the actual load.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
};

SearchSuggestLoaderImpl::AuthenticatedURLLoader::AuthenticatedURLLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL api_url,
    LoadDoneCallback callback)
    : url_loader_factory_(url_loader_factory),
      api_url_(std::move(api_url)),
      callback_(std::move(callback)) {}

void SearchSuggestLoaderImpl::AuthenticatedURLLoader::Start() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("search_suggest_service", R"(
        semantics {
          sender: "Search Suggestion Service"
          description:
            "Downloads search suggestions to be shown on the New Tab Page to "
            "logged-in users based on their previous search history."
          trigger:
            "Displaying the new tab page, if Google is the "
            "configured search provider, and the user is signed in."
          data: "Google credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'. Users can "
            "opt out of this feature using a button attached to the suggestions."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = api_url_;
  variations::AppendVariationsHeaderUnknownSignedIn(
      api_url_, variations::InIncognito::kNo, resource_request.get());
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &SearchSuggestLoaderImpl::AuthenticatedURLLoader::OnURLLoaderComplete,
          base::Unretained(this)),
      1024 * 1024);
}

void SearchSuggestLoaderImpl::AuthenticatedURLLoader::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  std::move(callback_).Run(simple_loader_.get(), std::move(response_body));
}

SearchSuggestLoaderImpl::SearchSuggestLoaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GoogleURLTracker* google_url_tracker,
    const std::string& application_locale)
    : url_loader_factory_(url_loader_factory),
      google_url_tracker_(google_url_tracker),
      application_locale_(application_locale),
      weak_ptr_factory_(this) {}

SearchSuggestLoaderImpl::~SearchSuggestLoaderImpl() = default;

void SearchSuggestLoaderImpl::Load(const std::string& blocklist,
                                   SearchSuggestionsCallback callback) {
  callbacks_.push_back(std::move(callback));

  // Note: If there is an ongoing request, abandon it. It's possible that
  // something has changed in the meantime (e.g. signin state) that would make
  // the result obsolete.
  pending_request_ = std::make_unique<AuthenticatedURLLoader>(
      url_loader_factory_, GetApiUrl(blocklist),
      base::BindOnce(&SearchSuggestLoaderImpl::LoadDone,
                     base::Unretained(this)));
  pending_request_->Start();
}

GURL SearchSuggestLoaderImpl::GetLoadURLForTesting() const {
  std::string blocklist;
  return GetApiUrl(blocklist);
}

GURL SearchSuggestLoaderImpl::GetApiUrl(const std::string& blocklist) const {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = google_url_tracker_->google_url();
  }

  GURL api_url = google_base_url.Resolve(kNewTabSearchSuggestionsApiPath);

  api_url = net::AppendQueryParameter(api_url, "vtgb", blocklist);

  return api_url;
}

void SearchSuggestLoaderImpl::LoadDone(
    const network::SimpleURLLoader* simple_loader,
    std::unique_ptr<std::string> response_body) {
  // The loader will be deleted when the request is handled.
  std::unique_ptr<AuthenticatedURLLoader> deleter(std::move(pending_request_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << simple_loader->NetError();
    Respond(Status::TRANSIENT_ERROR, base::nullopt);
    return;
  }

  std::string response;
  response.swap(*response_body);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kSearchSuggestResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kSearchSuggestResponsePreamble));
  }

  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      response,
      base::BindRepeating(&SearchSuggestLoaderImpl::JsonParsed,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&SearchSuggestLoaderImpl::JsonParseFailed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SearchSuggestLoaderImpl::JsonParsed(std::unique_ptr<base::Value> value) {
  base::Optional<SearchSuggestData> result = JsonToSearchSuggestionData(*value);
  Respond(result.has_value() ? Status::OK : Status::FATAL_ERROR, result);
}

void SearchSuggestLoaderImpl::JsonParseFailed(const std::string& message) {
  DVLOG(1) << "Parsing JSON failed: " << message;
  Respond(Status::FATAL_ERROR, base::nullopt);
}

void SearchSuggestLoaderImpl::Respond(
    Status status,
    const base::Optional<SearchSuggestData>& data) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(status, data);
  }
  callbacks_.clear();
}
