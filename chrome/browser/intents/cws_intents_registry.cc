// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/cws_intents_registry.h"

#include "base/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/api_key.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/url_util.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"

namespace {

// Limit for the number of suggestions we fix from CWS. Ideally, the registry
// simply get all of them, but there is a) chunking on the CWS side, and b)
// there is a cost with suggestions fetched. (Network overhead for favicons,
// roundtrips to registry to check if installed).
//
// Since the picker limits the number of suggestions displayed to 5, 15 means
// the suggestion list only has the potential to be shorter than that once the
// user has at least 10 installed handlers for the particular action/type.
//
// TODO(groby): Adopt number of suggestions dynamically so the picker can
// always display 5 suggestions unless there are less than 5 viable extensions
// in the CWS.
const char kMaxSuggestions[] = "15";

// URL for CWS intents API.
const char kCWSIntentServiceURL[] =
  "https://www.googleapis.com/chromewebstore/v1.1b/items/intent";

// Parses a JSON |response| from the CWS into a list of suggested extensions,
// stored in |intents|. |intents| must not be NULL.
void ParseResponse(const std::string& response,
                   CWSIntentsRegistry::IntentExtensionList* intents) {
  std::string error;
  scoped_ptr<Value> parsed_response;
  JSONStringValueSerializer serializer(response);
  parsed_response.reset(serializer.Deserialize(NULL, &error));
  if (parsed_response.get() == NULL)
    return;

  DictionaryValue* response_dict = NULL;
  if (!parsed_response->GetAsDictionary(&response_dict) || !response_dict)
    return;

  ListValue* items;
  if (!response_dict->GetList("items", &items))
    return;

  for (ListValue::const_iterator iter(items->begin());
       iter != items->end(); ++iter) {
    DictionaryValue* item = static_cast<DictionaryValue*>(*iter);

    // All fields are mandatory - skip this result if any field isn't found.
    CWSIntentsRegistry::IntentExtensionInfo info;
    if (!item->GetString("id", &info.id))
      continue;

    if (!item->GetInteger("num_ratings", &info.num_ratings))
      continue;

    if (!item->GetDouble("average_rating", &info.average_rating))
      continue;

    if (!item->GetString("manifest", &info.manifest))
      continue;

    std::string manifest_utf8 = UTF16ToUTF8(info.manifest);
    JSONStringValueSerializer manifest_serializer(manifest_utf8);
    scoped_ptr<Value> manifest_value;
    manifest_value.reset(manifest_serializer.Deserialize(NULL, &error));
    if (manifest_value.get() == NULL)
      continue;

    DictionaryValue* manifest_dict;
    if (!manifest_value->GetAsDictionary(&manifest_dict) ||
        !manifest_dict->GetString("name", &info.name))
      continue;

    string16 url_string;
    if (!item->GetString("icon_url", &url_string))
      continue;
    info.icon_url = GURL(url_string);

    intents->push_back(info);
  }
}

}  // namespace

// Internal object representing all data associated with a single query.
struct CWSIntentsRegistry::IntentsQuery {
  IntentsQuery();
  ~IntentsQuery();

  // Underlying URL request query.
  scoped_ptr<net::URLFetcher> url_fetcher;

  // The callback - invoked on completed retrieval.
  ResultsCallback callback;
};

CWSIntentsRegistry::IntentsQuery::IntentsQuery() {
}

CWSIntentsRegistry::IntentsQuery::~IntentsQuery() {
}

CWSIntentsRegistry::IntentExtensionInfo::IntentExtensionInfo()
    : num_ratings(0),
      average_rating(0) {
}

CWSIntentsRegistry::IntentExtensionInfo::~IntentExtensionInfo() {
}

CWSIntentsRegistry::CWSIntentsRegistry(net::URLRequestContextGetter* context)
    : request_context_(context) {
}

CWSIntentsRegistry::~CWSIntentsRegistry() {
  // Cancel all pending queries, since we can't handle them any more.
  STLDeleteValues(&queries_);
}

void CWSIntentsRegistry::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);

  URLFetcherHandle handle = reinterpret_cast<URLFetcherHandle>(source);
  QueryMap::iterator it = queries_.find(handle);
  DCHECK(it != queries_.end());
  scoped_ptr<IntentsQuery> query(it->second);
  DCHECK(query.get() != NULL);
  queries_.erase(it);

  std::string response;
  source->GetResponseAsString(&response);

  // TODO(groby): Do we really only accept 200, or any 2xx codes?
  IntentExtensionList intents;
  if (source->GetResponseCode() == 200)
    ParseResponse(response, &intents);

  if (!query->callback.is_null())
    query->callback.Run(intents);
}

void CWSIntentsRegistry::GetIntentServices(const string16& action,
                                           const string16& mimetype,
                                           const ResultsCallback& cb) {
  scoped_ptr<IntentsQuery> query(new IntentsQuery);
  query->callback = cb;
  query->url_fetcher.reset(content::URLFetcher::Create(
      0, BuildQueryURL(action,mimetype), net::URLFetcher::GET, this));

  if (query->url_fetcher.get() == NULL)
    return;

  query->url_fetcher->SetRequestContext(request_context_);
  query->url_fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);

  URLFetcherHandle handle = reinterpret_cast<URLFetcherHandle>(
      query->url_fetcher.get());
  queries_[handle] = query.release();
  queries_[handle]->url_fetcher->Start();
}

// static
GURL CWSIntentsRegistry::BuildQueryURL(const string16& action,
                                       const string16& type) {
  GURL request(kCWSIntentServiceURL);
  request = chrome_common_net::AppendQueryParameter(request, "intent",
                                                    UTF16ToUTF8(action));
  request = chrome_common_net::AppendQueryParameter(request, "mime_types",
                                                    UTF16ToUTF8(type));
  request = chrome_common_net::AppendQueryParameter(request, "start_index",
                                                    "0");
  request = chrome_common_net::AppendQueryParameter(request, "num_results",
                                                    kMaxSuggestions);
  if (web_intents::kApiKey[0]) {
    request = chrome_common_net::AppendQueryParameter(request, "key",
                                                       web_intents::kApiKey);
  }

  return request;
}
