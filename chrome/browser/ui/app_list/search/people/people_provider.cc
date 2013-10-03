// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/people/people_provider.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/app_list/search/common/json_response_fetcher.h"
#include "chrome/browser/ui/app_list/search/people/people_result.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const char kKeyItems[] = "items";
const char kKeyId[] = "person.id";
const char kKeyNames[] = "person.names";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmails[] = "person.emails";
const char kKeyEmailValue[] = "value";
const char kKeySortKeys[] = "person.sortKeys";
const char kKeyInteractionRank[] = "interactionRank";
const char kKeyImages[] = "person.images";
const char kKeyUrl[] = "url";

const char kAccessTokenField[] = "access_token";
const char kQueryField[] = "query";
const char kPeopleSearchUrl[] =
    "https://www.googleapis.com/plus/v2whitelisted/people/autocomplete";

// OAuth2 scope for access to the Google+ People Search API.
const char kPeopleSearchOAuth2Scope[] =
    "https://www.googleapis.com/auth/plus.peopleapi.readwrite";

// Get's the value associated with the key in the first dictionary in the list.
std::string GetFirstValue(const ListValue& list, const char key[]) {
  ListValue::const_iterator it = list.begin();
  if (it == list.end())
    return std::string();

  base::DictionaryValue* dict;
  if (!(*it)->GetAsDictionary(&dict))
    return std::string();

  std::string value;
  if (!dict || !dict->GetString(key, &value))
    return std::string();

  return value;
}

}  // namespace

PeopleProvider::PeopleProvider(Profile* profile)
  : WebserviceSearchProvider(profile),
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

  query_ = UTF16ToUTF8(query);
  const base::DictionaryValue* cached_result = cache_.Get(query_);
  if (cached_result) {
    ProcessPeopleSearchResults(cached_result);
    if (!people_search_fetched_callback_.is_null())
      people_search_fetched_callback_.Run();
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
  access_token_request_ = token_service->StartRequest(
      token_service->GetPrimaryAccountId(), oauth2_scope_, this);
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
  cache_.Put(query_, json.Pass());

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
  for (ListValue::const_iterator it = item_list->begin();
       it != item_list->end();
       ++it) {
    const base::DictionaryValue* dict;
    if (!(*it)->GetAsDictionary(&dict))
      continue;

    scoped_ptr<ChromeSearchResult> result(CreateResult(*dict));
    if (!result)
      continue;

    Add(result.Pass());
  }
}

scoped_ptr<ChromeSearchResult> PeopleProvider::CreateResult(
    const base::DictionaryValue& dict) {
  scoped_ptr<ChromeSearchResult> result;

  std::string id;
  if (!dict.GetString(kKeyId, &id))
    return result.Pass();

  // Get the display name.
  const base::ListValue* names;
  if (!dict.GetList(kKeyNames, &names))
    return result.Pass();
  std::string display_name;
  display_name = GetFirstValue(*names, kKeyDisplayName);

  // Get the email.
  const base::ListValue* emails;
  if (!dict.GetList(kKeyEmails, &emails))
    return result.Pass();
  std::string email;
  email = GetFirstValue(*emails, kKeyEmailValue);

  // Get the interaction rank.
  const base::DictionaryValue* sort_keys;
  if (!dict.GetDictionary(kKeySortKeys, &sort_keys))
    return result.Pass();
  std::string interaction_rank_string;
  if (!sort_keys->GetString(kKeyInteractionRank, &interaction_rank_string))
    return result.Pass();

  double interaction_rank;
  if (!base::StringToDouble(interaction_rank_string, &interaction_rank))
    return result.Pass();

  // If there has been no interaction with this user, the result
  // is meaningless, hence discard it.
  if (interaction_rank == 0.0)
    return result.Pass();

  // Get the image URL.
  const base::ListValue* images;
  if (!dict.GetList(kKeyImages, &images))
    return result.Pass();
  std::string image_url_string;
  image_url_string = GetFirstValue(*images, kKeyUrl);

  if (id.empty() ||
      display_name.empty() ||
      email.empty() ||
      interaction_rank_string.empty() ||
      image_url_string.empty()) {
    return result.Pass();
  }

  GURL image_url(image_url_string);
  if (!image_url.is_valid())
    return result.Pass();

  result.reset(new PeopleResult(
      profile_, id, display_name, email, interaction_rank, image_url));
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
