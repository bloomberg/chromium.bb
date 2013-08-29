// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore_provider.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/app_list/search/common/json_response_fetcher.h"
#include "chrome/browser/ui/app_list/search/search_webstore_result.h"
#include "chrome/browser/ui/app_list/search/webstore_result.h"
#include "chrome/common/extensions/extension_constants.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const char kKeyResults[] = "results";
const char kKeyId[] = "id";
const char kKeyLocalizedName[] = "localized_name";
const char kKeyIconUrl[] = "icon_url";
const size_t kMinimumQueryLength = 3u;

// Returns true if the launcher should send queries to the web store server.
bool UseWebstoreSearch() {
  const char kFieldTrialName[] = "LauncherUseWebstoreSearch";
  const char kEnable[] = "Enable";
  return base::FieldTrialList::FindFullName(kFieldTrialName) == kEnable;
}

}  // namespace

WebstoreProvider::WebstoreProvider(Profile* profile,
                                   AppListControllerDelegate* controller)
  : profile_(profile),
    controller_(controller) {}

WebstoreProvider::~WebstoreProvider() {}

void WebstoreProvider::Start(const base::string16& query) {
  ClearResults();

  // If |query| contains sensitive data, bail out and do not create the place
  // holder "search-web-store" result.
  if (IsSensitiveInput(query)) {
    query_.clear();
    return;
  }

  const std::string query_utf8 = UTF16ToUTF8(query);

  if (query_utf8.size() < kMinimumQueryLength) {
    query_.clear();
    return;
  }

  query_ = query_utf8;
  const base::DictionaryValue* cached_result = cache_.Get(query_);
  if (cached_result) {
    ProcessWebstoreSearchResults(cached_result);
    if (!webstore_search_fetched_callback_.is_null())
      webstore_search_fetched_callback_.Run();
    return;
  }

  if (UseWebstoreSearch() && chrome::IsSuggestPrefEnabled(profile_)) {
    if (!webstore_search_) {
      webstore_search_.reset(new JSONResponseFetcher(
          base::Bind(&WebstoreProvider::OnWebstoreSearchFetched,
                     base::Unretained(this)),
          profile_->GetRequestContext()));
    }

    StartThrottledQuery(base::Bind(&WebstoreProvider::StartQuery,
                                   base::Unretained(this)));
  }

  // Add a placeholder result which when clicked will run the user's query in a
  // browser. This placeholder is removed when the search results arrive.
  Add(scoped_ptr<ChromeSearchResult>(
      new SearchWebstoreResult(profile_, query_utf8)).Pass());
}

void WebstoreProvider::Stop() {
  if (webstore_search_)
    webstore_search_->Stop();
}

void WebstoreProvider::StartQuery() {
  // |query_| can be NULL when the query is scheduled but then canceled.
  if (!webstore_search_ || query_.empty())
    return;

  webstore_search_->Start(extension_urls::GetWebstoreJsonSearchUrl(
      query_, g_browser_process->GetApplicationLocale()));
}

void WebstoreProvider::OnWebstoreSearchFetched(
    scoped_ptr<base::DictionaryValue> json) {
  ProcessWebstoreSearchResults(json.get());
  cache_.Put(query_, json.Pass());

  if (!webstore_search_fetched_callback_.is_null())
    webstore_search_fetched_callback_.Run();
}

void WebstoreProvider::ProcessWebstoreSearchResults(
    const base::DictionaryValue* json) {
  const base::ListValue* result_list = NULL;
  if (!json ||
      !json->GetList(kKeyResults, &result_list) ||
      !result_list ||
      result_list->empty()) {
    return;
  }

  bool first_result = true;
  for (ListValue::const_iterator it = result_list->begin();
       it != result_list->end();
       ++it) {
    const base::DictionaryValue* dict;
    if (!(*it)->GetAsDictionary(&dict))
      continue;

    scoped_ptr<ChromeSearchResult> result(CreateResult(*dict));
    if (!result)
      continue;

    if (first_result) {
      // Clears "search in webstore" place holder results.
      ClearResults();
      first_result = false;
    }

    Add(result.Pass());
  }
}

scoped_ptr<ChromeSearchResult> WebstoreProvider::CreateResult(
    const base::DictionaryValue& dict) {
  scoped_ptr<ChromeSearchResult> result;

  std::string app_id;
  std::string localized_name;
  std::string icon_url_string;
  if (!dict.GetString(kKeyId, &app_id) ||
      !dict.GetString(kKeyLocalizedName, &localized_name) ||
      !dict.GetString(kKeyIconUrl, &icon_url_string)) {
    return result.Pass();
  }

  GURL icon_url(icon_url_string);
  if (!icon_url.is_valid())
    return result.Pass();

  result.reset(new WebstoreResult(
      profile_, app_id, localized_name, icon_url, controller_));
  return result.Pass();
}

}  // namespace app_list
