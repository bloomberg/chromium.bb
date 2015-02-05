// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/supervised_user_async_url_checker.h"

#include <string>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {

const char kQueryFormat[] = "https://www.googleapis.com/customsearch/v1"
    "?cx=%s&key=%s&q=inurl%%3A%s";
const char kQuerySafeParam[] = "&safe=high";

const char kIdSearchInfo[] = "searchInformation";
const char kIdResultCount[] = "totalResults";
const char kIdResults[] = "items";
const char kIdResultURL[] = "link";

const size_t kDefaultCacheSize = 1000;

// Build a normalized version of |url| for comparisons. Sets the scheme to a
// common default and strips a leading "www." from the host.
GURL GetNormalizedURL(const GURL& url) {
  GURL::Replacements replacements;
  // Set scheme to http.
  replacements.SetSchemeStr(url::kHttpScheme);
  // Strip leading "www." (if any).
  const std::string www("www.");
  const std::string host(url.host());
  if (StartsWithASCII(host, www, true))
    replacements.SetHostStr(base::StringPiece(host).substr(www.size()));
  // Strip trailing slash (if any).
  const std::string path(url.path());
  if (EndsWith(path, "/", true))
    replacements.SetPathStr(base::StringPiece(path).substr(0, path.size() - 1));
  return url.ReplaceComponents(replacements);
}

// Builds a URL for a web search for |url| (using the "inurl:" query parameter
// and a Custom Search Engine identified by |cx|, using the specified
// |api_key|). If |safe| is specified, enables the SafeSearch query parameter.
GURL BuildSearchURL(const std::string& cx,
                    const std::string& api_key,
                    const GURL& url,
                    bool safe) {
  // Strip the scheme, so that we'll match any scheme.
  std::string query = net::EscapeQueryParamValue(url.GetContent(), true);
  std::string search_url = base::StringPrintf(
      kQueryFormat,
      cx.c_str(),
      api_key.c_str(),
      query.c_str());
  if (safe)
    search_url.append(kQuerySafeParam);
  return GURL(search_url);
}

// Creates a URLFetcher for a Google web search for |url|. If |safe| is
// specified, enables SafeSearch for this request.
scoped_ptr<net::URLFetcher> CreateFetcher(
    URLFetcherDelegate* delegate,
    URLRequestContextGetter* context,
    const std::string& cx,
    const std::string& api_key,
    const GURL& url,
    bool safe) {
  const int kSafeId = 0;
  const int kUnsafeId = 1;
  int id = safe ? kSafeId : kUnsafeId;
  scoped_ptr<net::URLFetcher> fetcher(URLFetcher::Create(
      id, BuildSearchURL(cx, api_key, url, safe), URLFetcher::GET, delegate));
  fetcher->SetRequestContext(context);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  return fetcher.Pass();
}

// Checks whether the search |response| (in JSON format) contains an entry for
// the given |url|.
bool ResponseContainsURL(const std::string& response, const GURL& url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  const base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    DLOG(WARNING) << "ResponseContainsURL failed to parse global dictionary";
    return false;
  }
  const base::DictionaryValue* search_info_dict = NULL;
  if (!dict->GetDictionary(kIdSearchInfo, &search_info_dict)) {
    DLOG(WARNING) << "ResponseContainsURL failed to parse search information";
    return false;
  }
  std::string result_count;
  if (!search_info_dict->GetString(kIdResultCount, &result_count)) {
    DLOG(WARNING) << "ResponseContainsURL failed to parse result count";
    return false;
  }
  if (result_count == "0")
    return false;
  const base::ListValue* results_list = NULL;
  if (!dict->GetList(kIdResults, &results_list)) {
    DLOG(WARNING) << "ResponseContainsURL failed to parse list of results";
    return false;
  }
  GURL url_normalized = GetNormalizedURL(url);
  for (const base::Value* entry : *results_list) {
    const base::DictionaryValue* result_dict = NULL;
    if (!entry->GetAsDictionary(&result_dict)) {
      DLOG(WARNING) << "ResponseContainsURL failed to parse result dictionary";
      return false;
    }
    std::string result_url;
    if (!result_dict->GetString(kIdResultURL, &result_url)) {
      DLOG(WARNING) << "ResponseContainsURL failed to parse URL from result";
      return false;
    }
    if (url_normalized == GetNormalizedURL(GURL(result_url)))
      return true;
  }
  return false;
}

}  // namespace

struct SupervisedUserAsyncURLChecker::Check {
  Check(const GURL& url,
        scoped_ptr<net::URLFetcher> fetcher_safe,
        scoped_ptr<net::URLFetcher> fetcher_unsafe,
        const CheckCallback& callback);
  ~Check();

  GURL url;
  scoped_ptr<net::URLFetcher> fetcher_safe;
  scoped_ptr<net::URLFetcher> fetcher_unsafe;
  std::vector<CheckCallback> callbacks;
  bool safe_done;
  bool unsafe_done;
  base::Time start_time;
};

SupervisedUserAsyncURLChecker::Check::Check(
    const GURL& url,
    scoped_ptr<net::URLFetcher> fetcher_safe,
    scoped_ptr<net::URLFetcher> fetcher_unsafe,
    const CheckCallback& callback)
    : url(url),
      fetcher_safe(fetcher_safe.Pass()),
      fetcher_unsafe(fetcher_unsafe.Pass()),
      callbacks(1, callback),
      safe_done(false),
      unsafe_done(false),
      start_time(base::Time::Now()) {
}

SupervisedUserAsyncURLChecker::Check::~Check() {}

SupervisedUserAsyncURLChecker::CheckResult::CheckResult(
    SupervisedUserURLFilter::FilteringBehavior behavior, bool uncertain)
  : behavior(behavior), uncertain(uncertain) {
}

SupervisedUserAsyncURLChecker::SupervisedUserAsyncURLChecker(
    URLRequestContextGetter* context,
    const std::string& cx)
    : context_(context), cx_(cx), cache_(kDefaultCacheSize) {
}

SupervisedUserAsyncURLChecker::SupervisedUserAsyncURLChecker(
    URLRequestContextGetter* context,
    const std::string& cx,
    size_t cache_size)
    : context_(context), cx_(cx), cache_(cache_size) {
}

SupervisedUserAsyncURLChecker::~SupervisedUserAsyncURLChecker() {}

bool SupervisedUserAsyncURLChecker::CheckURL(const GURL& url,
                                             const CheckCallback& callback) {
  // TODO(treib): Hack: For now, allow all Google URLs to save search QPS. If we
  // ever remove this, we should find a way to allow at least the NTP.
  if (google_util::IsGoogleDomainUrl(url,
                                     google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    callback.Run(url, SupervisedUserURLFilter::ALLOW, false);
    return true;
  }
  // TODO(treib): Hack: For now, allow all YouTube URLs since YouTube has its
  // own Safety Mode anyway.
  if (google_util::IsYoutubeDomainUrl(url,
                                      google_util::ALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    callback.Run(url, SupervisedUserURLFilter::ALLOW, false);
    return true;
  }

  auto cache_it = cache_.Get(url);
  if (cache_it != cache_.end()) {
    const CheckResult& result = cache_it->second;
    DVLOG(1) << "Cache hit! " << url.spec() << " is "
             << (result.behavior == SupervisedUserURLFilter::BLOCK ? "NOT" : "")
             << " safe; certain: " << !result.uncertain;
    callback.Run(url, result.behavior, result.uncertain);
    return true;
  }

  // See if we already have a check in progress for this URL.
  for (Check* check : checks_in_progress_) {
    if (check->url == url) {
      DVLOG(1) << "Adding to pending check for " << url.spec();
      check->callbacks.push_back(callback);
      return false;
    }
  }

  DVLOG(1) << "Checking URL " << url;
  std::string api_key = google_apis::GetSafeSitesAPIKey();
  scoped_ptr<URLFetcher> fetcher_safe(
      CreateFetcher(this, context_, cx_, api_key, url, true));
  scoped_ptr<URLFetcher> fetcher_unsafe(
      CreateFetcher(this, context_, cx_, api_key, url, false));
  fetcher_safe->Start();
  fetcher_unsafe->Start();
  checks_in_progress_.push_back(
      new Check(url, fetcher_safe.Pass(), fetcher_unsafe.Pass(), callback));
  return false;
}

void SupervisedUserAsyncURLChecker::OnURLFetchComplete(
    const net::URLFetcher* source) {
  ScopedVector<Check>::iterator it = checks_in_progress_.begin();
  bool is_safe_search_request = false;
  while (it != checks_in_progress_.end()) {
    if (source == (*it)->fetcher_safe.get()) {
      is_safe_search_request = true;
      (*it)->safe_done = true;
      break;
    } else if (source == (*it)->fetcher_unsafe.get()) {
      (*it)->unsafe_done = true;
      break;
    }
    ++it;
  }
  DCHECK(it != checks_in_progress_.end());
  Check* check = *it;

  const URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    for (size_t i = 0; i < check->callbacks.size(); i++)
      check->callbacks[i].Run(check->url, SupervisedUserURLFilter::ALLOW, true);
    checks_in_progress_.erase(it);
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  bool url_in_search_result = ResponseContainsURL(response_body, check->url);

  // We consider a URL as safe if it turns up in a safesearch query. To handle
  // URLs that aren't in the search index at all, we also allows URLS that don't
  // turn up even in a non-safesearch query.
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::ALLOW;
  bool uncertain = true;
  if (is_safe_search_request) {
    if (url_in_search_result) {
      // Found the URL with safesearch, don't block.
      DVLOG(1) << check->url.spec() << " is safe, allowing.";
      behavior = SupervisedUserURLFilter::ALLOW;
      uncertain = false;
    } else if (check->unsafe_done) {
      // Found the URL only without safesearch, block.
      DVLOG(1) << check->url.spec() << " is NOT safe, blocking.";
      behavior = SupervisedUserURLFilter::BLOCK;
      uncertain = false;
    } else {
      // Didn't find the URL with safesearch, have to wait for non-safe result.
      return;
    }
  } else {
    if (!url_in_search_result) {
      // Didn't find the URL even without safesearch, have to let through.
      DVLOG(1) << check->url.spec() << " is unknown, allowing.";
      behavior = SupervisedUserURLFilter::ALLOW;
      uncertain = true;
    } else if (check->safe_done) {
      // Found the URL only without safesearch, block.
      DVLOG(1) << check->url.spec() << " is NOT safe, blocking.";
      behavior = SupervisedUserURLFilter::BLOCK;
      uncertain = false;
    } else {
      // Found the URL without safesearch, wait for safe result.
      return;
    }
  }

  UMA_HISTOGRAM_TIMES("ManagedUsers.SafeSitesDelay",
                      base::Time::Now() - check->start_time);

  cache_.Put(check->url, CheckResult(behavior, uncertain));

  for (size_t i = 0; i < check->callbacks.size(); i++)
    check->callbacks[i].Run(check->url, behavior, uncertain);
  checks_in_progress_.erase(it);
}
