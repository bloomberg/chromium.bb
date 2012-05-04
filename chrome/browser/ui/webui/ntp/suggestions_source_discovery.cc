// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_source_discovery.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace {

const char kResponseDataValue[] = "responseData";
const char kResultsValue[] = "results";
const char kUnescapedUrlValue[] = "unescapedUrl";
const char kDiscoveryBackendURL[] =
    "https://ajax.googleapis.com/ajax/services/search/news?v=1.0&q=a&rsz=8";

// The weight used by the combiner to determine which ratio of suggestions
// should be obtained from this source.
const int kSuggestionsDiscoveryWeight = 1;

}  // namespace

SuggestionsSourceDiscovery::SuggestionsSourceDiscovery() : combiner_(NULL) {
}

SuggestionsSourceDiscovery::~SuggestionsSourceDiscovery() {
  STLDeleteElements(&items_);
}

inline int SuggestionsSourceDiscovery::GetWeight() {
  return kSuggestionsDiscoveryWeight;
}

int SuggestionsSourceDiscovery::GetItemCount() {
  return items_.size();
}

DictionaryValue* SuggestionsSourceDiscovery::PopItem() {
  if (items_.empty())
    return NULL;
  DictionaryValue* item = items_.front();
  items_.pop_front();
  return item;
}

void SuggestionsSourceDiscovery::FetchItems(Profile* profile) {
  DCHECK(combiner_);

  // If a fetch is already in progress, we don't have to start a new one.
  if (recommended_fetcher_ != NULL)
    return;

  // TODO(beaudoin): Extract the URL to some preference. Use a better service.
  recommended_fetcher_.reset(content::URLFetcher::Create(
      GURL(kDiscoveryBackendURL), content::URLFetcher::GET, this));
  recommended_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  recommended_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SAVE_COOKIES);
  recommended_fetcher_->Start();
}

void SuggestionsSourceDiscovery::SetCombiner(SuggestionsCombiner* combiner) {
  DCHECK(!combiner_);
  combiner_ = combiner;
}

void SuggestionsSourceDiscovery::OnURLFetchComplete(
    const content::URLFetcher* source) {
  DCHECK(combiner_);
  STLDeleteElements(&items_);
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    scoped_ptr<Value> message_value(base::JSONReader::Read(data, false));

    DictionaryValue* response_dict;
    DictionaryValue* response_data;
    ListValue* results;
    if (message_value.get() &&
        message_value->GetAsDictionary(&response_dict) &&
        response_dict->GetDictionary(kResponseDataValue, &response_data) &&
        response_data->GetList(kResultsValue, &results)) {
      for (size_t i = 0; i < results->GetSize(); ++i) {
        DictionaryValue* result;
        if (!results->GetDictionary(i, &result) || !result)
          continue;

        string16 unescaped_url;
        if (!result->GetString(kUnescapedUrlValue, &unescaped_url))
          continue;

        DictionaryValue* page_value = new DictionaryValue();
        NewTabUI::SetURLTitleAndDirection(page_value,
                                          unescaped_url,
                                          GURL(unescaped_url));
        items_.push_back(page_value);
      }
    }
  }

  recommended_fetcher_.reset();
  combiner_->OnItemsReady();
}
