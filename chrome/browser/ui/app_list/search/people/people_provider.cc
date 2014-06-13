// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/people/people_provider.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/search/common/json_response_fetcher.h"
#include "chrome/browser/ui/app_list/search/people/people_result.h"
#include "chrome/browser/ui/app_list/search/people/person.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const char kKeyItems[] = "items";

const char kAccessTokenField[] = "access_token";
const char kQueryField[] = "query";
const char kPeopleSearchUrl[] =
    "https://www.googleapis.com/plus/v2whitelisted/people/autocomplete";

// OAuth2 scope for access to the Google+ People Search API.
const char kPeopleSearchOAuth2Scope[] =
    "https://www.googleapis.com/auth/plus.peopleapi.readwrite";

}  // namespace

PeopleProvider::PeopleProvider(Profile* profile)
  : WebserviceSearchProvider(profile),
    OAuth2TokenService::Consumer("people_provider"),
    people_search_url_(kPeopleSearchUrl),
    skip_request_token_for_test_(false) {
  oauth2_scope_.insert(kPeopleSearchOAuth2Scope);
}

PeopleProvider::~PeopleProvider() {}

void PeopleProvider::Start(const base::string16& query) {
  ClearResults();
  if (!IsValidQuery(query)) {
    query_.clear();
    return;
  }

  query_ = base::UTF16ToUTF8(query);

  const CacheResult result = cache_->Get(WebserviceCache::PEOPLE, query_);
  if (result.second) {
    ProcessPeopleSearchResults(result.second);
    if (!people_search_fetched_callback_.is_null())
      people_search_fetched_callback_.Run();
    if (result.first == FRESH)
      return;
  }


  if (!people_search_) {
    people_search_.reset(new JSONResponseFetcher(
        base::Bind(&PeopleProvider::OnPeopleSearchFetched,
                   base::Unretained(this)),
        profile_->GetRequestContext()));
  }

  if (!skip_request_token_for_test_) {
    // We start with reqesting the access token. Once the token is fetched,
    // we'll create the full query URL and fetch it.
    StartThrottledQuery(base::Bind(&PeopleProvider::RequestAccessToken,
                                   base::Unretained(this)));
  } else {
    // Running in a test, skip requesting the access token, straight away
    // start our query.
    StartThrottledQuery(base::Bind(&PeopleProvider::StartQuery,
                                   base::Unretained(this)));
  }
}

void PeopleProvider::Stop() {
  if (people_search_)
    people_search_->Stop();
}

void PeopleProvider::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_, request);
  access_token_request_.reset();
  access_token_ = access_token;
  StartQuery();
}

void PeopleProvider::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_, request);
  access_token_request_.reset();
}

void PeopleProvider::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_request_ != NULL)
    return;

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile_);
  access_token_request_ = token_service->StartRequest(
      signin_manager->GetAuthenticatedAccountId(), oauth2_scope_, this);
}

GURL PeopleProvider::GetQueryUrl(const std::string& query) {
  GURL people_search_url = people_search_url_;
  people_search_url = net::AppendQueryParameter(people_search_url,
                                                kAccessTokenField,
                                                access_token_);
  people_search_url = net::AppendQueryParameter(people_search_url,
                                                kQueryField,
                                                query);

  return people_search_url;
}

void PeopleProvider::StartQuery() {
  // |query_| can be NULL when the query is scheduled but then canceled.
  if (!people_search_ || query_.empty())
    return;

  GURL url = GetQueryUrl(query_);
  people_search_->Start(url);
}

void PeopleProvider::OnPeopleSearchFetched(
    scoped_ptr<base::DictionaryValue> json) {
  ProcessPeopleSearchResults(json.get());
  cache_->Put(WebserviceCache::PEOPLE, query_, json.Pass());

  if (!people_search_fetched_callback_.is_null())
    people_search_fetched_callback_.Run();
}

void PeopleProvider::ProcessPeopleSearchResults(
    const base::DictionaryValue* json) {
  const base::ListValue* item_list = NULL;
  if (!json ||
      !json->GetList(kKeyItems, &item_list) ||
      !item_list ||
      item_list->empty()) {
    return;
  }

  ClearResults();
  for (base::ListValue::const_iterator it = item_list->begin();
       it != item_list->end();
       ++it) {
    const base::DictionaryValue* dict;
    if (!(*it)->GetAsDictionary(&dict))
      continue;

    scoped_ptr<SearchResult> result(CreateResult(*dict));
    if (!result)
      continue;

    Add(result.Pass());
  }
}

scoped_ptr<ChromeSearchResult> PeopleProvider::CreateResult(
    const base::DictionaryValue& dict) {
  scoped_ptr<ChromeSearchResult> result;

  scoped_ptr<Person> person = Person::Create(dict);
  if (!person)
    return result.Pass();

  result.reset(new PeopleResult(profile_, person.Pass()));
  return result.Pass();
}

void PeopleProvider::SetupForTest(
    const base::Closure& people_search_fetched_callback,
    const GURL& people_search_url) {
  people_search_fetched_callback_ = people_search_fetched_callback;
  people_search_url_ = people_search_url;
  skip_request_token_for_test_ = true;
}

}  // namespace app_list
