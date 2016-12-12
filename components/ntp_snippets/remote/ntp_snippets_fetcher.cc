// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_fetcher.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "ui/base/l10n/l10n_util.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;
using translate::LanguageModel;

namespace ntp_snippets {

namespace {

const char kChromeReaderApiScope[] =
    "https://www.googleapis.com/auth/webhistory";
const char kContentSuggestionsApiScope[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kSnippetsServerNonAuthorizedFormat[] = "%s?key=%s";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

// Variation parameter for personalizing fetching of snippets.
const char kPersonalizationName[] = "fetching_personalization";

// Variation parameter for disabling the retry.
const char kBackground5xxRetriesName[] = "background_5xx_retries_count";

// Variation parameter for chrome-content-suggestions backend.
const char kContentSuggestionsBackend[] = "content_suggestions_backend";

// Constants for possible values of the "fetching_personalization" parameter.
const char kPersonalizationPersonalString[] = "personal";
const char kPersonalizationNonPersonalString[] = "non_personal";
const char kPersonalizationBothString[] = "both";  // the default value

const int kMaxExcludedIds = 100;

// Variation parameter for sending LanguageModel info to the server.
const char kSendTopLanguagesName[] = "send_top_languages";

// Variation parameter for sending UserClassifier info to the server.
const char kSendUserClassName[] = "send_user_class";

const char kBooleanParameterEnabled[] = "true";
const char kBooleanParameterDisabled[] = "false";

const int kFetchTimeHistogramResolution = 5;

std::string FetchResultToString(NTPSnippetsFetcher::FetchResult result) {
  switch (result) {
    case NTPSnippetsFetcher::FetchResult::SUCCESS:
      return "OK";
    case NTPSnippetsFetcher::FetchResult::DEPRECATED_EMPTY_HOSTS:
      return "Cannot fetch for empty hosts list.";
    case NTPSnippetsFetcher::FetchResult::URL_REQUEST_STATUS_ERROR:
      return "URLRequestStatus error";
    case NTPSnippetsFetcher::FetchResult::HTTP_ERROR:
      return "HTTP error";
    case NTPSnippetsFetcher::FetchResult::JSON_PARSE_ERROR:
      return "Received invalid JSON";
    case NTPSnippetsFetcher::FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
      return "Invalid / empty list.";
    case NTPSnippetsFetcher::FetchResult::OAUTH_TOKEN_ERROR:
      return "Error in obtaining an OAuth2 access token.";
    case NTPSnippetsFetcher::FetchResult::INTERACTIVE_QUOTA_ERROR:
      return "Out of interactive quota.";
    case NTPSnippetsFetcher::FetchResult::NON_INTERACTIVE_QUOTA_ERROR:
      return "Out of non-interactive quota.";
    case NTPSnippetsFetcher::FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return "Unknown error";
}

std::string GetFetchEndpoint() {
  std::string endpoint = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kContentSuggestionsBackend);
  return endpoint.empty() ? kChromeReaderServer : endpoint;
}

bool IsBooleanParameterEnabled(const std::string& param_name,
                               bool default_value) {
  std::string param_value = variations::GetVariationParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, param_name);
  if (param_value == kBooleanParameterEnabled) {
    return true;
  }
  if (param_value == kBooleanParameterDisabled) {
    return false;
  }
  if (!param_value.empty()) {
    LOG(WARNING) << "Invalid value \"" << param_value
                 << "\" for variation parameter " << param_name;
  }
  return default_value;
}

int Get5xxRetryCount(bool interactive_request) {
  if (interactive_request) {
    return 2;
  }
  return std::max(0, variations::GetVariationParamByFeatureAsInt(
                         ntp_snippets::kArticleSuggestionsFeature,
                         kBackground5xxRetriesName, 0));
}

bool UsesChromeContentSuggestionsAPI(const GURL& endpoint) {
  if (endpoint == kChromeReaderServer) {
    return false;
  }

  if (endpoint != kContentSuggestionsServer &&
      endpoint != kContentSuggestionsStagingServer &&
      endpoint != kContentSuggestionsAlphaServer) {
    LOG(WARNING) << "Unknown value for " << kContentSuggestionsBackend << ": "
                 << "assuming chromecontentsuggestions-style API";
  }
  return true;
}

// Creates snippets from dictionary values in |list| and adds them to
// |snippets|. Returns true on success, false if anything went wrong.
// |remote_category_id| is only used if |content_suggestions_api| is true.
bool AddSnippetsFromListValue(bool content_suggestions_api,
                              int remote_category_id,
                              const base::ListValue& list,
                              NTPSnippet::PtrVector* snippets) {
  for (const auto& value : list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict)) {
      return false;
    }

    std::unique_ptr<NTPSnippet> snippet;
    if (content_suggestions_api) {
      snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(
          *dict, remote_category_id);
    } else {
      snippet = NTPSnippet::CreateFromChromeReaderDictionary(*dict);
    }
    if (!snippet) {
      return false;
    }

    snippets->push_back(std::move(snippet));
  }
  return true;
}

// Translate the BCP 47 |language_code| into a posix locale string.
std::string PosixLocaleFromBCP47Language(const std::string& language_code) {
  char locale[ULOC_FULLNAME_CAPACITY];
  UErrorCode error = U_ZERO_ERROR;
  // Translate the input to a posix locale.
  uloc_forLanguageTag(language_code.c_str(), locale, ULOC_FULLNAME_CAPACITY,
                      nullptr, &error);
  if (error != U_ZERO_ERROR) {
    DLOG(WARNING) << "Error in translating language code to a locale string: "
                  << error;
    return std::string();
  }
  return locale;
}

std::string ISO639FromPosixLocale(const std::string& locale) {
  char language[ULOC_LANG_CAPACITY];
  UErrorCode error = U_ZERO_ERROR;
  uloc_getLanguage(locale.c_str(), language, ULOC_LANG_CAPACITY, &error);
  if (error != U_ZERO_ERROR) {
    DLOG(WARNING)
        << "Error in translating locale string to a ISO639 language code: "
        << error;
    return std::string();
  }
  return language;
}

void AppendLanguageInfoToList(base::ListValue* list,
                              const LanguageModel::LanguageInfo& info) {
  auto lang = base::MakeUnique<base::DictionaryValue>();
  lang->SetString("language", info.language_code);
  lang->SetDouble("frequency", info.frequency);
  list->Append(std::move(lang));
}

std::string GetUserClassString(UserClassifier::UserClass user_class) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return "RARE_NTP_USER";
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return "ACTIVE_NTP_USER";
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return "ACTIVE_SUGGESTIONS_CONSUMER";
  }
  NOTREACHED();
  return std::string();
}

int GetMinuteOfTheDay(bool local_time, bool reduced_resolution) {
  base::Time now(base::Time::Now());
  base::Time::Exploded now_exploded{};
  local_time ? now.LocalExplode(&now_exploded) : now.UTCExplode(&now_exploded);
  int now_minute = reduced_resolution
                       ? now_exploded.minute / kFetchTimeHistogramResolution *
                             kFetchTimeHistogramResolution
                       : now_exploded.minute;
  return now_exploded.hour * 60 + now_minute;
}

// The response from the backend might include suggestions from multiple
// categories. If only a single category was requested, this function filters
// all other categories out.
void FilterCategories(NTPSnippetsFetcher::FetchedCategoriesVector* categories,
                      base::Optional<Category> exclusive_category) {
  if (!exclusive_category.has_value()) {
    return;
  }
  Category exclusive = exclusive_category.value();
  auto category_it = std::find_if(
      categories->begin(), categories->end(),
      [&exclusive](const NTPSnippetsFetcher::FetchedCategory& c) -> bool {
        return c.category == exclusive;
      });
  if (category_it == categories->end()) {
    categories->clear();
    return;
  }
  NTPSnippetsFetcher::FetchedCategory category = std::move(*category_it);
  categories->clear();
  categories->push_back(std::move(category));
}

}  // namespace

// A single request to query snippets.
class NTPSnippetsFetcher::JsonRequest : public net::URLFetcherDelegate {
 public:
  JsonRequest(base::Optional<Category> exclusive_category,
              base::TickClock* tick_clock,
              const ParseJSONCallback& callback);
  JsonRequest(JsonRequest&&);
  ~JsonRequest() override;

  // A client can expect error_details only, if there was any error during the
  // fetching or parsing. In successful cases, it will be an empty string.
  using CompletedCallback =
      base::OnceCallback<void(std::unique_ptr<base::Value> result,
                              FetchResult result_code,
                              const std::string& error_details)>;

  void Start(CompletedCallback callback);

  const base::Optional<Category>& exclusive_category() const {
    return exclusive_category_;
  }

  base::TimeDelta GetFetchDuration() const;
  std::string GetResponseString() const;

 private:
  friend class RequestBuilder;

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void ParseJsonResponse();
  void OnJsonParsed(std::unique_ptr<base::Value> result);
  void OnJsonError(const std::string& error);

  // The fetcher for downloading the snippets. Only non-null if a fetch is
  // currently ongoing.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // If set, only return results for this category.
  base::Optional<Category> exclusive_category_;

  // Use the TickClock from the Fetcher to measure the fetch time. It will be
  // used on creation and after the fetch returned. It has to be alive until the
  // request is destroyed.
  base::TickClock* tick_clock_;
  base::TimeTicks creation_time_;

  // This callback is called to parse a json string. It contains callbacks for
  // error and success cases.
  ParseJSONCallback parse_json_callback_;

  // The callback to notify when URLFetcher finished and results are available.
  CompletedCallback request_completed_callback_;

  base::WeakPtrFactory<JsonRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(JsonRequest);
};

CategoryInfo BuildArticleCategoryInfo(
    const base::Optional<base::string16>& title) {
  return CategoryInfo(
      title.has_value() ? title.value()
                        : l10n_util::GetStringUTF16(
                              IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::FULL_CARD,
      // TODO(dgn): merge has_more_action and has_reload_action when we remove
      // the kFetchMoreFeature flag. See https://crbug.com/667752
      /*has_more_action=*/base::FeatureList::IsEnabled(kFetchMoreFeature),
      /*has_reload_action=*/true,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/true,
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

CategoryInfo BuildRemoteCategoryInfo(const base::string16& title,
                                     bool allow_fetching_more_results) {
  return CategoryInfo(
      title, ContentSuggestionsCardLayout::FULL_CARD,
      // TODO(dgn): merge has_more_action and has_reload_action when we remove
      // the kFetchMoreFeature flag. See https://crbug.com/667752
      /*has_more_action=*/allow_fetching_more_results &&
          base::FeatureList::IsEnabled(kFetchMoreFeature),
      /*has_reload_action=*/allow_fetching_more_results,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/false,
      // TODO(tschumann): The message for no-articles is likely wrong
      // and needs to be added to the stubby protocol if we want to
      // support it.
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

NTPSnippetsFetcher::FetchedCategory::FetchedCategory(Category c,
                                                     CategoryInfo&& info)
    : category(c), info(info) {}

NTPSnippetsFetcher::FetchedCategory::FetchedCategory(FetchedCategory&&) =
    default;

NTPSnippetsFetcher::FetchedCategory::~FetchedCategory() = default;

NTPSnippetsFetcher::FetchedCategory& NTPSnippetsFetcher::FetchedCategory::
operator=(FetchedCategory&&) = default;

NTPSnippetsFetcher::Params::Params() = default;
NTPSnippetsFetcher::Params::Params(const Params&) = default;
NTPSnippetsFetcher::Params::~Params() = default;

NTPSnippetsFetcher::NTPSnippetsFetcher(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    CategoryFactory* category_factory,
    LanguageModel* language_model,
    const ParseJSONCallback& parse_json_callback,
    const std::string& api_key,
    const UserClassifier* user_classifier)
    : OAuth2TokenService::Consumer("ntp_snippets"),
      signin_manager_(signin_manager),
      token_service_(token_service),
      url_request_context_getter_(std::move(url_request_context_getter)),
      category_factory_(category_factory),
      language_model_(language_model),
      parse_json_callback_(parse_json_callback),
      fetch_url_(GetFetchEndpoint()),
      fetch_api_(UsesChromeContentSuggestionsAPI(fetch_url_)
                     ? FetchAPI::CHROME_CONTENT_SUGGESTIONS_API
                     : FetchAPI::CHROME_READER_API),
      api_key_(api_key),
      tick_clock_(new base::DefaultTickClock()),
      user_classifier_(user_classifier),
      request_throttler_rare_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER),
      request_throttler_active_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER),
      request_throttler_active_suggestions_consumer_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER),
      weak_ptr_factory_(this) {
  std::string personalization = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kPersonalizationName);
  if (personalization == kPersonalizationNonPersonalString) {
    personalization_ = Personalization::kNonPersonal;
  } else if (personalization == kPersonalizationPersonalString) {
    personalization_ = Personalization::kPersonal;
  } else {
    personalization_ = Personalization::kBoth;
    LOG_IF(WARNING, !personalization.empty() &&
                        personalization != kPersonalizationBothString)
        << "Unknown value for " << kPersonalizationName << ": "
        << personalization;
  }
}

NTPSnippetsFetcher::~NTPSnippetsFetcher() {
  if (waiting_for_refresh_token_) {
    token_service_->RemoveObserver(this);
  }
}

void NTPSnippetsFetcher::FetchSnippets(const Params& params,
                                       SnippetsAvailableCallback callback) {
  if (!DemandQuotaForRequest(params.interactive_request)) {
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  params.interactive_request
                      ? FetchResult::INTERACTIVE_QUOTA_ERROR
                      : FetchResult::NON_INTERACTIVE_QUOTA_ERROR,
                  /*error_details=*/std::string());
    return;
  }

  if (!params.interactive_request) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.FetchTimeLocal",
                                GetMinuteOfTheDay(/*local_time=*/true,
                                                  /*reduced_resolution=*/true));
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.FetchTimeUTC",
                                GetMinuteOfTheDay(/*local_time=*/false,
                                                  /*reduced_resolution=*/true));
  }

  RequestBuilder builder;
  builder.SetFetchAPI(fetch_api_)
      .SetFetchAPI(fetch_api_)
      .SetLanguageModel(language_model_)
      .SetParams(params)
      .SetParseJsonCallback(parse_json_callback_)
      .SetPersonalization(personalization_)
      .SetTickClock(tick_clock_.get())
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUserClassifier(*user_classifier_);

  if (NeedsAuthentication() && signin_manager_->IsAuthenticated()) {
    // Signed-in: get OAuth token --> fetch snippets.
    oauth_token_retried_ = false;
    pending_requests_.emplace(std::move(builder), std::move(callback));
    StartTokenRequest();
  } else if (NeedsAuthentication() && signin_manager_->AuthInProgress()) {
    // Currently signing in: wait for auth to finish (the refresh token) -->
    //     get OAuth token --> fetch snippets.
    pending_requests_.emplace(std::move(builder), std::move(callback));
    if (!waiting_for_refresh_token_) {
      // Wait until we get a refresh token.
      waiting_for_refresh_token_ = true;
      token_service_->AddObserver(this);
    }
  } else {
    // Not signed in: fetch snippets (without authentication).
    FetchSnippetsNonAuthenticated(std::move(builder), std::move(callback));
  }
}

NTPSnippetsFetcher::JsonRequest::JsonRequest(
    base::Optional<Category> exclusive_category,
    base::TickClock* tick_clock,  // Needed until destruction of the request.
    const ParseJSONCallback& callback)
    : exclusive_category_(exclusive_category),
      tick_clock_(tick_clock),
      parse_json_callback_(callback),
      weak_ptr_factory_(this) {
  creation_time_ = tick_clock_->NowTicks();
}

NTPSnippetsFetcher::JsonRequest::~JsonRequest() {
  LOG_IF(DFATAL, !request_completed_callback_.is_null())
      << "The CompletionCallback was never called!";
}

void NTPSnippetsFetcher::JsonRequest::Start(CompletedCallback callback) {
  request_completed_callback_ = std::move(callback);
  url_fetcher_->Start();
}

base::TimeDelta NTPSnippetsFetcher::JsonRequest::GetFetchDuration() const {
  return tick_clock_->NowTicks() - creation_time_;
}

std::string NTPSnippetsFetcher::JsonRequest::GetResponseString() const {
  std::string response;
  url_fetcher_->GetResponseAsString(&response);
  return response;
}

////////////////////////////////////////////////////////////////////////////////
// URLFetcherDelegate overrides
void NTPSnippetsFetcher::JsonRequest::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);
  const URLRequestStatus& status = url_fetcher_->GetStatus();
  int response = url_fetcher_->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "NewTabPage.Snippets.FetchHttpResponseOrErrorCode",
      status.is_success() ? response : status.error());

  if (!status.is_success()) {
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::URL_REQUEST_STATUS_ERROR,
             /*error_details=*/base::StringPrintf(" %d", status.error()));
  } else if (response != net::HTTP_OK) {
    // TODO(jkrcal): https://crbug.com/609084
    // We need to deal with the edge case again where the auth
    // token expires just before we send the request (in which case we need to
    // fetch a new auth token). We should extract that into a common class
    // instead of adding it to every single class that uses auth tokens.
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::HTTP_ERROR,
             /*error_details=*/base::StringPrintf(" %d", response));
  } else {
    ParseJsonResponse();
  }
}

void NTPSnippetsFetcher::JsonRequest::ParseJsonResponse() {
  std::string json_string;
  bool stores_result_to_string =
      url_fetcher_->GetResponseAsString(&json_string);
  DCHECK(stores_result_to_string);

  parse_json_callback_.Run(
      json_string,
      base::Bind(&JsonRequest::OnJsonParsed, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&JsonRequest::OnJsonError, weak_ptr_factory_.GetWeakPtr()));
}

void NTPSnippetsFetcher::JsonRequest::OnJsonParsed(
    std::unique_ptr<base::Value> result) {
  std::move(request_completed_callback_)
      .Run(std::move(result), FetchResult::SUCCESS,
           /*error_details=*/std::string());
}

void NTPSnippetsFetcher::JsonRequest::OnJsonError(const std::string& error) {
  std::string json_string;
  url_fetcher_->GetResponseAsString(&json_string);
  LOG(WARNING) << "Received invalid JSON (" << error << "): " << json_string;
  std::move(request_completed_callback_)
      .Run(/*result=*/nullptr, FetchResult::JSON_PARSE_ERROR,
           /*error_details=*/base::StringPrintf(" (error %s)", error.c_str()));
}

NTPSnippetsFetcher::RequestBuilder::RequestBuilder()
    : fetch_api_(CHROME_READER_API),
      personalization_(Personalization::kBoth),
      language_model_(nullptr) {}
NTPSnippetsFetcher::RequestBuilder::RequestBuilder(RequestBuilder&&) = default;
NTPSnippetsFetcher::RequestBuilder::~RequestBuilder() = default;

std::unique_ptr<NTPSnippetsFetcher::JsonRequest>
NTPSnippetsFetcher::RequestBuilder::Build() const {
  DCHECK(!url_.is_empty());
  DCHECK(url_request_context_getter_);
  auto request = base::MakeUnique<JsonRequest>(
      params_.exclusive_category, tick_clock_, parse_json_callback_);
  std::string body = BuildBody();
  std::string headers = BuildHeaders();
  request->url_fetcher_ = BuildURLFetcher(request.get(), headers, body);

  // Log the request for debugging network issues.
  VLOG(1) << "Sending a NTP snippets request to " << url_ << ":\n"
          << headers << "\n"
          << body;

  return request;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetAuthentication(
    const std::string& account_id,
    const std::string& auth_header) {
  obfuscated_gaia_id_ = account_id;
  auth_header_ = auth_header;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetFetchAPI(FetchAPI fetch_api) {
  fetch_api_ = fetch_api;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetLanguageModel(
    const translate::LanguageModel* language_model) {
  language_model_ = language_model;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetParams(const Params& params) {
  params_ = params;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetParseJsonCallback(
    ParseJSONCallback callback) {
  parse_json_callback_ = callback;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetPersonalization(
    Personalization personalization) {
  personalization_ = personalization;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetTickClock(base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder& NTPSnippetsFetcher::RequestBuilder::SetUrl(
    const GURL& url) {
  url_ = url;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetUrlRequestContextGetter(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter) {
  url_request_context_getter_ = context_getter;
  return *this;
}

NTPSnippetsFetcher::RequestBuilder&
NTPSnippetsFetcher::RequestBuilder::SetUserClassifier(
    const UserClassifier& user_classifier) {
  if (IsSendingUserClassEnabled()) {
    user_class_ = GetUserClassString(user_classifier.GetUserClass());
  }
  return *this;
}

bool NTPSnippetsFetcher::RequestBuilder::IsSendingTopLanguagesEnabled() const {
  return IsBooleanParameterEnabled(kSendTopLanguagesName,
                                   /*default_value=*/false);
}

bool NTPSnippetsFetcher::RequestBuilder::IsSendingUserClassEnabled() const {
  return IsBooleanParameterEnabled(kSendUserClassName,
                                   /*default_value=*/false);
}

std::string NTPSnippetsFetcher::RequestBuilder::BuildHeaders() const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  if (!auth_header_.empty()) {
    headers.SetHeader("Authorization", auth_header_);
  }
  // Add X-Client-Data header with experiment IDs from field trials.
  // Note: It's fine to pass in |is_signed_in| false, which does not affect
  // transmission of experiment ids coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(url_,
                                     false,  // incognito
                                     false,  // uma_enabled
                                     is_signed_in, &headers);
  return headers.ToString();
}

std::string NTPSnippetsFetcher::RequestBuilder::BuildBody() const {
  auto request = base::MakeUnique<base::DictionaryValue>();
  std::string user_locale = PosixLocaleFromBCP47Language(params_.language_code);
  switch (fetch_api_) {
    case NTPSnippetsFetcher::CHROME_READER_API: {
      auto content_params = base::MakeUnique<base::DictionaryValue>();
      content_params->SetBoolean("only_return_personalized_results",
                                 ReturnOnlyPersonalizedResults());

      auto content_restricts = base::MakeUnique<base::ListValue>();
      for (const auto* metadata : {"TITLE", "SNIPPET", "THUMBNAIL"}) {
        auto entry = base::MakeUnique<base::DictionaryValue>();
        entry->SetString("type", "METADATA");
        entry->SetString("value", metadata);
        content_restricts->Append(std::move(entry));
      }

      auto content_selectors = base::MakeUnique<base::ListValue>();
      for (const auto& host : params_.hosts) {
        auto entry = base::MakeUnique<base::DictionaryValue>();
        entry->SetString("type", "HOST_RESTRICT");
        entry->SetString("value", host);
        content_selectors->Append(std::move(entry));
      }

      auto local_scoring_params = base::MakeUnique<base::DictionaryValue>();
      local_scoring_params->Set("content_params", std::move(content_params));
      local_scoring_params->Set("content_restricts",
                                std::move(content_restricts));
      local_scoring_params->Set("content_selectors",
                                std::move(content_selectors));

      auto global_scoring_params = base::MakeUnique<base::DictionaryValue>();
      global_scoring_params->SetInteger("num_to_return",
                                        params_.count_to_fetch);
      global_scoring_params->SetInteger("sort_type", 1);

      auto advanced = base::MakeUnique<base::DictionaryValue>();
      advanced->Set("local_scoring_params", std::move(local_scoring_params));
      advanced->Set("global_scoring_params", std::move(global_scoring_params));

      request->SetString("response_detail_level", "STANDARD");
      request->Set("advanced_options", std::move(advanced));
      if (!obfuscated_gaia_id_.empty()) {
        request->SetString("obfuscated_gaia_id", obfuscated_gaia_id_);
      }
      if (!user_locale.empty()) {
        request->SetString("user_locale", user_locale);
      }
      break;
    }

    case NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API: {
      if (!user_locale.empty()) {
        request->SetString("uiLanguage", user_locale);
      }

      auto regular_hosts = base::MakeUnique<base::ListValue>();
      for (const auto& host : params_.hosts) {
        regular_hosts->AppendString(host);
      }
      request->Set("regularlyVisitedHostNames", std::move(regular_hosts));
      request->SetString("priority", params_.interactive_request
                                         ? "USER_ACTION"
                                         : "BACKGROUND_PREFETCH");

      auto excluded = base::MakeUnique<base::ListValue>();
      for (const auto& id : params_.excluded_ids) {
        excluded->AppendString(id);
        if (excluded->GetSize() >= kMaxExcludedIds) {
          break;
        }
      }
      request->Set("excludedSuggestionIds", std::move(excluded));

      if (!user_class_.empty()) {
        request->SetString("userActivenessClass", user_class_);
      }

      translate::LanguageModel::LanguageInfo ui_language;
      translate::LanguageModel::LanguageInfo other_top_language;
      PrepareLanguages(&ui_language, &other_top_language);

      if (ui_language.frequency == 0 && other_top_language.frequency == 0) {
        break;
      }

      auto language_list = base::MakeUnique<base::ListValue>();
      if (ui_language.frequency > 0) {
        AppendLanguageInfoToList(language_list.get(), ui_language);
      }
      if (other_top_language.frequency > 0) {
        AppendLanguageInfoToList(language_list.get(), other_top_language);
      }
      request->Set("topLanguages", std::move(language_list));

      // TODO(sfiera): Support only_return_personalized_results.
      // TODO(sfiera): Support count_to_fetch.
      break;
    }
  }

  std::string request_json;
  bool success = base::JSONWriter::WriteWithOptions(
      *request, base::JSONWriter::OPTIONS_PRETTY_PRINT, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<net::URLFetcher>
NTPSnippetsFetcher::RequestBuilder::BuildURLFetcher(
    net::URLFetcherDelegate* delegate,
    const std::string& headers,
    const std::string& body) const {
  std::unique_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(url_, net::URLFetcher::POST, delegate);
  url_fetcher->SetRequestContext(url_request_context_getter_.get());
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher.get(), data_use_measurement::DataUseUserData::NTP_SNIPPETS);

  url_fetcher->SetExtraRequestHeaders(headers);
  url_fetcher->SetUploadData("application/json", body);

  // Fetchers are sometimes cancelled because a network change was detected.
  url_fetcher->SetAutomaticallyRetryOnNetworkChanges(3);
  url_fetcher->SetMaxRetriesOn5xx(
      Get5xxRetryCount(params_.interactive_request));
  return url_fetcher;
}

void NTPSnippetsFetcher::RequestBuilder::PrepareLanguages(
    translate::LanguageModel::LanguageInfo* ui_language,
    translate::LanguageModel::LanguageInfo* other_top_language) const {
  // TODO(jkrcal): Add language model factory for iOS and add fakes to tests so
  // that |language_model| is never nullptr. Remove this check and add a DCHECK
  // into the constructor.
  if (!language_model_ || !IsSendingTopLanguagesEnabled()) {
    return;
  }

  // TODO(jkrcal): Is this back-and-forth converting necessary?
  ui_language->language_code = ISO639FromPosixLocale(
      PosixLocaleFromBCP47Language(params_.language_code));
  ui_language->frequency =
      language_model_->GetLanguageFrequency(ui_language->language_code);

  std::vector<LanguageModel::LanguageInfo> top_languages =
      language_model_->GetTopLanguages();
  for (const LanguageModel::LanguageInfo& info : top_languages) {
    if (info.language_code != ui_language->language_code) {
      *other_top_language = info;

      // Report to UMA how important the UI language is.
      DCHECK_GT(other_top_language->frequency, 0)
          << "GetTopLanguages() should not return languages with 0 frequency";
      float ratio_ui_in_both_languages =
          ui_language->frequency /
          (ui_language->frequency + other_top_language->frequency);
      UMA_HISTOGRAM_PERCENTAGE(
          "NewTabPage.Languages.UILanguageRatioInTwoTopLanguages",
          ratio_ui_in_both_languages * 100);
      break;
    }
  }
}

void NTPSnippetsFetcher::FetchSnippetsNonAuthenticated(
    RequestBuilder builder,
    SnippetsAvailableCallback callback) {
  // When not providing OAuth token, we need to pass the Google API key.
  builder.SetUrl(
      GURL(base::StringPrintf(kSnippetsServerNonAuthorizedFormat,
                              fetch_url_.spec().c_str(), api_key_.c_str())));
  StartRequest(std::move(builder), std::move(callback));
}

void NTPSnippetsFetcher::FetchSnippetsAuthenticated(
    RequestBuilder builder,
    SnippetsAvailableCallback callback,
    const std::string& account_id,
    const std::string& oauth_access_token) {
  // TODO(jkrcal, treib): Add unit-tests for authenticated fetches.
  builder.SetUrl(fetch_url_)
      .SetAuthentication(account_id,
                         base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                            oauth_access_token.c_str()));
  StartRequest(std::move(builder), std::move(callback));
}

void NTPSnippetsFetcher::StartRequest(RequestBuilder builder,
                                      SnippetsAvailableCallback callback) {
  std::unique_ptr<JsonRequest> request = builder.Build();
  JsonRequest* raw_request = request.get();
  raw_request->Start(base::BindOnce(&NTPSnippetsFetcher::JsonRequestDone,
                                    base::Unretained(this), std::move(request),
                                    std::move(callback)));
}

void NTPSnippetsFetcher::StartTokenRequest() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(fetch_api_ == FetchAPI::CHROME_CONTENT_SUGGESTIONS_API
                    ? kContentSuggestionsApiScope
                    : kChromeReaderApiScope);
  oauth_request_ = token_service_->StartRequest(
      signin_manager_->GetAuthenticatedAccountId(), scopes, this);
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Consumer overrides
void NTPSnippetsFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Delete the request after we leave this method.
  std::unique_ptr<OAuth2TokenService::Request> oauth_request(
      std::move(oauth_request_));
  DCHECK_EQ(oauth_request.get(), request)
      << "Got tokens from some previous request";

  while (!pending_requests_.empty()) {
    std::pair<RequestBuilder, SnippetsAvailableCallback> builder_and_callback =
        std::move(pending_requests_.front());
    pending_requests_.pop();
    FetchSnippetsAuthenticated(std::move(builder_and_callback.first),
                               std::move(builder_and_callback.second),
                               oauth_request->GetAccountId(), access_token);
  }
}

void NTPSnippetsFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  oauth_request_.reset();

  if (!oauth_token_retried_ &&
      error.state() == GoogleServiceAuthError::State::REQUEST_CANCELED) {
    // The request (especially on startup) can get reset by loading the refresh
    // token - do it one more time.
    oauth_token_retried_ = true;
    StartTokenRequest();
    return;
  }

  DLOG(ERROR) << "Unable to get token: " << error.ToString();
  while (!pending_requests_.empty()) {
    std::pair<RequestBuilder, SnippetsAvailableCallback> builder_and_callback =
        std::move(pending_requests_.front());

    FetchFinished(OptionalFetchedCategories(),
                  std::move(builder_and_callback.second),
                  FetchResult::OAUTH_TOKEN_ERROR,
                  /*error_details=*/base::StringPrintf(
                      " (%s)", error.ToString().c_str()));
    pending_requests_.pop();
  }
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Observer overrides
void NTPSnippetsFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Only react on tokens for the account the user has signed in with.
  if (account_id != signin_manager_->GetAuthenticatedAccountId()) {
    return;
  }

  token_service_->RemoveObserver(this);
  waiting_for_refresh_token_ = false;
  oauth_token_retried_ = false;
  StartTokenRequest();
}

void NTPSnippetsFetcher::JsonRequestDone(std::unique_ptr<JsonRequest> request,
                                         SnippetsAvailableCallback callback,
                                         std::unique_ptr<base::Value> result,
                                         FetchResult status_code,
                                         const std::string& error_details) {
  DCHECK(request);
  last_fetch_json_ = request->GetResponseString();

  UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime",
                      request->GetFetchDuration());

  if (!result) {
    FetchFinished(OptionalFetchedCategories(), std::move(callback), status_code,
                  error_details);
    return;
  }
  FetchedCategoriesVector categories;
  if (!JsonToSnippets(*result, &categories)) {
    LOG(WARNING) << "Received invalid snippets: " << last_fetch_json_;
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR, std::string());
    return;
  }
  // Filter out unwanted categories if necessary.
  // TODO(fhorschig): As soon as the server supports filtering by category,
  // adjust the request instead of over-fetching and filtering here.
  FilterCategories(&categories, request->exclusive_category());

  FetchFinished(std::move(categories), std::move(callback),
                FetchResult::SUCCESS, std::string());
}

void NTPSnippetsFetcher::FetchFinished(OptionalFetchedCategories categories,
                                       SnippetsAvailableCallback callback,
                                       FetchResult status_code,
                                       const std::string& error_details) {
  DCHECK(status_code == FetchResult::SUCCESS || !categories.has_value());

  last_status_ = FetchResultToString(status_code) + error_details;

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.FetchResult",
                            static_cast<int>(status_code),
                            static_cast<int>(FetchResult::RESULT_MAX));

  DVLOG(1) << "Fetch finished: " << last_status_;

  std::move(callback).Run(status_code, std::move(categories));
}

bool NTPSnippetsFetcher::JsonToSnippets(const base::Value& parsed,
                                        FetchedCategoriesVector* categories) {
  const base::DictionaryValue* top_dict = nullptr;
  if (!parsed.GetAsDictionary(&top_dict)) {
    return false;
  }

  switch (fetch_api_) {
    case FetchAPI::CHROME_READER_API: {
      const int kUnusedRemoteCategoryId = -1;
      categories->push_back(FetchedCategory(
          category_factory_->FromKnownCategory(KnownCategories::ARTICLES),
          BuildArticleCategoryInfo(base::nullopt)));

      const base::ListValue* recos = nullptr;
      return top_dict->GetList("recos", &recos) &&
             AddSnippetsFromListValue(/*content_suggestions_api=*/false,
                                      kUnusedRemoteCategoryId, *recos,
                                      &categories->back().snippets);
    }

    case FetchAPI::CHROME_CONTENT_SUGGESTIONS_API: {
      const base::ListValue* categories_value = nullptr;
      if (!top_dict->GetList("categories", &categories_value)) {
        return false;
      }

      for (const auto& v : *categories_value) {
        std::string utf8_title;
        int remote_category_id = -1;
        const base::DictionaryValue* category_value = nullptr;
        if (!(v->GetAsDictionary(&category_value) &&
              category_value->GetString("localizedTitle", &utf8_title) &&
              category_value->GetInteger("id", &remote_category_id) &&
              (remote_category_id > 0))) {
          return false;
        }

        NTPSnippet::PtrVector snippets;
        const base::ListValue* suggestions = nullptr;
        // Absence of a list of suggestions is treated as an empty list, which
        // is permissible.
        if (category_value->GetList("suggestions", &suggestions)) {
          if (!AddSnippetsFromListValue(
                  /*content_suggestions_api=*/true, remote_category_id,
                  *suggestions, &snippets)) {
            return false;
          }
        }
        Category category =
            category_factory_->FromRemoteCategory(remote_category_id);
        if (category.IsKnownCategory(KnownCategories::ARTICLES)) {
          categories->push_back(FetchedCategory(
              category,
              BuildArticleCategoryInfo(base::UTF8ToUTF16(utf8_title))));
        } else {
          // TODO(tschumann): Right now, the backend does not yet populate this
          // field. Make it mandatory once the backends provide it.
          bool allow_fetching_more_results = false;
          category_value->GetBoolean("allowFetchingMoreResults",
                                     &allow_fetching_more_results);
          categories->push_back(FetchedCategory(
              category, BuildRemoteCategoryInfo(base::UTF8ToUTF16(utf8_title),
                                                allow_fetching_more_results)));
        }
        categories->back().snippets = std::move(snippets);
      }
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool NTPSnippetsFetcher::DemandQuotaForRequest(bool interactive_request) {
  switch (user_classifier_->GetUserClass()) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return request_throttler_rare_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return request_throttler_active_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return request_throttler_active_suggestions_consumer_
          .DemandQuotaForRequest(interactive_request);
  }
  NOTREACHED();
  return false;
}

bool NTPSnippetsFetcher::NeedsAuthentication() const {
  return (personalization_ == Personalization::kPersonal ||
          personalization_ == Personalization::kBoth);
}

}  // namespace ntp_snippets
