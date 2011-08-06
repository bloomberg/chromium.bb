// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"

// Internal object representing all data associated with a single query.
struct WebIntentsRegistry::IntentsQuery {
  // Unique query identifier.
  QueryID query_id_;

  // Underlying data query.
  WebDataService::Handle pending_query_;

  // the consumer for this particular query.
  Consumer* consumer_;

  // TODO(groby): Additional filter data will go here - filtering is handled
  // per query.
};

WebIntentsRegistry::WebIntentsRegistry() : next_query_id_(0) {}

WebIntentsRegistry::~WebIntentsRegistry() {
  // Cancel all pending queries, since we can't handle them any more.
  for (QueryMap::iterator it(queries.begin()); it != queries.end(); ++it) {
    wds_->CancelRequest(it->first);
    delete it->second;
  }
}

void WebIntentsRegistry::Initialize(scoped_refptr<WebDataService> wds) {
  wds_ = wds;
}

void WebIntentsRegistry::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_RESULT);

  QueryMap::iterator it = queries.find(h);
  DCHECK(it != queries.end());

  IntentsQuery* query(it->second);
  DCHECK(query);
  queries.erase(it);

  // TODO(groby): Filtering goes here.
  std::vector<WebIntentData> intents = static_cast<
      const WDResult<std::vector<WebIntentData> >*>(result)->GetValue();

  query->consumer_->OnIntentsQueryDone(query->query_id_, intents);
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetIntentProviders(
    const string16& action,
    Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->consumer_ = consumer;
  query->pending_query_ = wds_->GetWebIntents(action, this);
  queries[query->pending_query_] = query;

  return query->query_id_;
}

void WebIntentsRegistry::RegisterIntentProvider(const WebIntentData& intent) {
  DCHECK(wds_.get());
  wds_->AddWebIntent(intent);
}

void WebIntentsRegistry::UnregisterIntentProvider(const WebIntentData& intent) {
  DCHECK(wds_.get());
  wds_->RemoveWebIntent(intent);
}
