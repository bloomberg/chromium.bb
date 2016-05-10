// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;

namespace ntp_snippets {

namespace {

const char kContentSnippetsServerFormat[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=%s";

const char kRequestParameterFormat[] =
    "{"
    "  \"response_detail_level\": \"STANDARD\","
    "  \"advanced_options\": {"
    "    \"local_scoring_params\": {"
    "      \"content_params\": {"
    "        \"only_return_personalized_results\": false"
    "      },"
    "      \"content_restricts\": {"
    "        \"type\": \"METADATA\","
    "        \"value\": \"TITLE\""
    "      },"
    "      \"content_restricts\": {"
    "        \"type\": \"METADATA\","
    "        \"value\": \"SNIPPET\""
    "      },"
    "      \"content_restricts\": {"
    "        \"type\": \"METADATA\","
    "        \"value\": \"THUMBNAIL\""
    "      }"
    "%s"
    "    },"
    "    \"global_scoring_params\": {"
    "      \"num_to_return\": %i"
    "    }"
    "  }"
    "}";

const char kHostRestrictFormat[] =
    "      ,\"content_selectors\": {"
    "        \"type\": \"HOST_RESTRICT\","
    "        \"value\": \"%s\""
    "      }";

std::string FetchResultToString(NTPSnippetsFetcher::FetchResult result) {
  switch (result) {
    case NTPSnippetsFetcher::FetchResult::SUCCESS:
      return "OK";
    case NTPSnippetsFetcher::FetchResult::EMPTY_HOSTS:
      return "Cannot fetch for empty hosts list.";
    case NTPSnippetsFetcher::FetchResult::URL_REQUEST_STATUS_ERROR:
      return "URLRequestStatus error";
    case NTPSnippetsFetcher::FetchResult::HTTP_ERROR:
      return "HTTP error";
    case NTPSnippetsFetcher::FetchResult::JSON_PARSE_ERROR:
      return "Received invalid JSON";
    case NTPSnippetsFetcher::FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
      return "Invalid / empty list.";
    case NTPSnippetsFetcher::FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return "Unknown error";
}

}  // namespace

NTPSnippetsFetcher::NTPSnippetsFetcher(
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    const ParseJSONCallback& parse_json_callback,
    bool is_stable_channel)
    : url_request_context_getter_(url_request_context_getter),
      parse_json_callback_(parse_json_callback),
      is_stable_channel_(is_stable_channel),
      weak_ptr_factory_(this) {}

NTPSnippetsFetcher::~NTPSnippetsFetcher() {}

void NTPSnippetsFetcher::SetCallback(
    const SnippetsAvailableCallback& callback) {
  snippets_available_callback_ = callback;
}

void NTPSnippetsFetcher::FetchSnippetsFromHosts(
    const std::set<std::string>& hosts, int count) {
  std::string host_restricts;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDontRestrict)) {
    if (hosts.empty()) {
      FetchFinished(OptionalSnippets(), FetchResult::EMPTY_HOSTS,
                    /*extra_message=*/std::string());
      return;
    }
    for (const std::string& host : hosts)
      host_restricts += base::StringPrintf(kHostRestrictFormat, host.c_str());
  }
  const std::string& key = is_stable_channel_
                               ? google_apis::GetAPIKey()
                               : google_apis::GetNonStableAPIKey();
  std::string url =
      base::StringPrintf(kContentSnippetsServerFormat, key.c_str());
  url_fetcher_ = URLFetcher::Create(GURL(url), URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(url_request_context_getter_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::NTP_SNIPPETS);
  HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  url_fetcher_->SetExtraRequestHeaders(headers.ToString());
  url_fetcher_->SetUploadData("application/json",
                              base::StringPrintf(kRequestParameterFormat,
                                                 host_restricts.c_str(),
                                                 count));

  // Fetchers are sometimes cancelled because a network change was detected.
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  // Try to make fetching the files bit more robust even with poor connection.
  url_fetcher_->SetMaxRetriesOn5xx(3);
  url_fetcher_->Start();
}

////////////////////////////////////////////////////////////////////////////////
// URLFetcherDelegate overrides
void NTPSnippetsFetcher::OnURLFetchComplete(const URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  const URLRequestStatus& status = source->GetStatus();

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "NewTabPage.Snippets.FetchHttpResponseOrErrorCode",
      status.is_success() ? source->GetResponseCode() : status.error());

  if (!status.is_success()) {
    FetchFinished(OptionalSnippets(), FetchResult::URL_REQUEST_STATUS_ERROR,
                  /*extra_message=*/base::StringPrintf(" %d", status.error()));
  } else if (source->GetResponseCode() != net::HTTP_OK) {
    FetchFinished(
        OptionalSnippets(), FetchResult::HTTP_ERROR,
        /*extra_message=*/base::StringPrintf(" %d", source->GetResponseCode()));
  } else {
    bool stores_result_to_string = source->GetResponseAsString(
        &last_fetch_json_);
    DCHECK(stores_result_to_string);

    parse_json_callback_.Run(
        last_fetch_json_,
        base::Bind(&NTPSnippetsFetcher::OnJsonParsed,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&NTPSnippetsFetcher::OnJsonError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void NTPSnippetsFetcher::OnJsonParsed(std::unique_ptr<base::Value> parsed) {
  const base::DictionaryValue* top_dict = nullptr;
  const base::ListValue* list = nullptr;
  NTPSnippet::PtrVector snippets;
  if (!parsed->GetAsDictionary(&top_dict) ||
      !top_dict->GetList("recos", &list) ||
      !NTPSnippet::AddFromListValue(*list, &snippets)) {
    LOG(WARNING) << "Received invalid snippets: " << last_fetch_json_;
    FetchFinished(OptionalSnippets(),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR,
                  /*extra_message=*/std::string());
  } else {
    FetchFinished(OptionalSnippets(std::move(snippets)), FetchResult::SUCCESS,
                  /*extra_message=*/std::string());
  }
}

void NTPSnippetsFetcher::OnJsonError(const std::string& error) {
  LOG(WARNING) << "Received invalid JSON (" << error << "): "
               << last_fetch_json_;
  FetchFinished(
      OptionalSnippets(), FetchResult::JSON_PARSE_ERROR,
      /*extra_message=*/base::StringPrintf(" (error %s)", error.c_str()));
}

void NTPSnippetsFetcher::FetchFinished(OptionalSnippets snippets,
                                       FetchResult result,
                                       const std::string& extra_message) {
  DCHECK(result == FetchResult::SUCCESS || !snippets);
  last_status_ = FetchResultToString(result) + extra_message;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.FetchResult",
                            static_cast<int>(result),
                            static_cast<int>(FetchResult::RESULT_MAX));
  if (!snippets_available_callback_.is_null())
    snippets_available_callback_.Run(std::move(snippets));
}

}  // namespace ntp_snippets
