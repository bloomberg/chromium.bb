// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore_provider.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/app_list/search/search_webstore_result.h"
#include "chrome/browser/ui/app_list/search/webstore_result.h"
#include "chrome/browser/ui/app_list/search/webstore_search_fetcher.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const char kKeyResults[] = "results";
const char kKeyId[] = "id";
const char kKeyLocalizedName[] = "localized_name";
const char kKeyIconUrl[] = "icon_url";
const size_t kMinimumQueryLength = 3u;
const int kWebstoreQueryThrottleIntrevalInMs = 100;

// Returns true if the launcher should send queries to the web store server.
bool UseWebstoreSearch() {
  const char kFieldTrialName[] = "LauncherUseWebstoreSearch";
  const char kEnable[] = "Enable";
  return base::FieldTrialList::FindFullName(kFieldTrialName) == kEnable;
}

// Returns whether or not the user's input string, |query|, might contain any
// sensitive information, based purely on its value and not where it came from.
bool IsSensitiveInput(const string16& query) {
  const GURL query_as_url(query);
  if (!query_as_url.is_valid())
    return false;

  // The input can be interpreted as a URL. Check to see if it is potentially
  // sensitive. (Code shamelessly copied from search_provider.cc's
  // IsQuerySuitableForSuggest function.)

  // First we check the scheme: if this looks like a URL with a scheme that is
  // file, we shouldn't send it. Sending such things is a waste of time and a
  // disclosure of potentially private, local data. If the scheme is OK, we
  // still need to check other cases below.
  if (LowerCaseEqualsASCII(query_as_url.scheme(), chrome::kFileScheme))
    return true;

  // Don't send URLs with usernames, queries or refs. Some of these are
  // private, and the Suggest server is unlikely to have any useful results
  // for any of them. Also don't send URLs with ports, as we may initially
  // think that a username + password is a host + port (and we don't want to
  // send usernames/passwords), and even if the port really is a port, the
  // server is once again unlikely to have and useful results.
  if (!query_as_url.username().empty() ||
      !query_as_url.port().empty() ||
      !query_as_url.query().empty() ||
      !query_as_url.ref().empty()) {
    return true;
  }

  // Don't send anything for https except the hostname. Hostnames are OK
  // because they are visible when the TCP connection is established, but the
  // specific path may reveal private information.
  if (LowerCaseEqualsASCII(query_as_url.scheme(), content::kHttpsScheme) &&
      !query_as_url.path().empty() && query_as_url.path() != "/") {
    return true;
  }

  return false;
}

}  // namespace

WebstoreProvider::WebstoreProvider(Profile* profile,
                                   AppListControllerDelegate* controller)
  : profile_(profile),
    controller_(controller),
    use_throttling_(true) {}

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
      webstore_search_.reset(new WebstoreSearchFetcher(
          base::Bind(&WebstoreProvider::OnWebstoreSearchFetched,
                     base::Unretained(this)),
          profile_->GetRequestContext()));
    }

    base::TimeDelta interval =
        base::TimeDelta::FromMilliseconds(kWebstoreQueryThrottleIntrevalInMs);
    if (!use_throttling_ || base::Time::Now() - last_keytyped_ > interval) {
      query_throttler_.Stop();
      StartQuery();
    } else {
      query_throttler_.Start(
          FROM_HERE,
          interval,
          base::Bind(&WebstoreProvider::StartQuery, base::Unretained(this)));
    }
    last_keytyped_ = base::Time::Now();
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

  webstore_search_->Start(query_, g_browser_process->GetApplicationLocale());
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
