// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "net/base/mime_util.h"

namespace {

// Compares two mime types for equality. Supports wild cards in both
// |type1| and |type2|. Wild cards are of the form '<type>/*' or '*'.
bool MimeTypesAreEqual(const string16& type1, const string16& type2) {
  // We don't have a MIME matcher that allows patterns on both sides
  // Instead, we do two comparison, treating each type in turn as a
  // pattern. If either one matches, we consider this a MIME match.
  if (net::MatchesMimeType(UTF16ToUTF8(type1), UTF16ToUTF8(type2)))
    return true;
  return net::MatchesMimeType(UTF16ToUTF8(type2), UTF16ToUTF8(type1));
}

}  // namespace

using webkit_glue::WebIntentServiceData;

// Internal object representing all data associated with a single query.
struct WebIntentsRegistry::IntentsQuery {
  // Unique query identifier.
  QueryID query_id_;

  // Underlying data query.
  WebDataService::Handle pending_query_;

  // The consumer for this particular query.
  Consumer* consumer_;

  // The particular action to filter for while searching through extensions.
  // If |action_| is empty, return all extension-provided intents.
  string16 action_;

  // The MIME type that was requested for this intent query.
  // Suppports wild cards.
  string16 type_;
};

WebIntentsRegistry::WebIntentsRegistry() : next_query_id_(0) {}

WebIntentsRegistry::~WebIntentsRegistry() {
  // Cancel all pending queries, since we can't handle them any more.
  for (QueryMap::iterator it(queries_.begin()); it != queries_.end(); ++it) {
    wds_->CancelRequest(it->first);
    delete it->second;
  }
}

void WebIntentsRegistry::Initialize(
    scoped_refptr<WebDataService> wds,
    ExtensionServiceInterface* extension_service) {
  wds_ = wds;
  extension_service_ = extension_service;
}

void WebIntentsRegistry::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_RESULT);

  QueryMap::iterator it = queries_.find(h);
  DCHECK(it != queries_.end());

  IntentsQuery* query(it->second);
  DCHECK(query);
  queries_.erase(it);

  IntentServiceList matching_services = static_cast<
      const WDResult<IntentServiceList>*>(result)->GetValue();

  // Loop over all intents in all extensions, collect ones matching the query.
  if (extension_service_) {
    const ExtensionList* extensions = extension_service_->extensions();
    if (extensions) {
      for (ExtensionList::const_iterator i(extensions->begin());
           i != extensions->end(); ++i) {
        const IntentServiceList& intents((*i)->intents_services());
        for (IntentServiceList::const_iterator j(intents.begin());
             j != intents.end(); ++j) {
          if (query->action_.empty() || query->action_ == j->action)
            matching_services.push_back(*j);
        }
      }
    }
  }

  // Filter out all intents not matching the query type.
  IntentServiceList::iterator iter(matching_services.begin());
  while (iter != matching_services.end()) {
    if (MimeTypesAreEqual(iter->type, query->type_))
      ++iter;
    else
      iter = matching_services.erase(iter);
  }

  query->consumer_->OnIntentsQueryDone(query->query_id_, matching_services);
  delete query;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetIntentProviders(
    const string16& action, const string16& mimetype, Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->consumer_ = consumer;
  query->action_ = action;
  query->type_ = mimetype;
  query->pending_query_ = wds_->GetWebIntentServices(action, this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetAllIntentProviders(
    Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->consumer_ = consumer;
  query->type_ = ASCIIToUTF16("*");
  query->pending_query_ = wds_->GetAllWebIntentServices(this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

// Trampoline consumer for calls to IntentProviderExists. Forwards existence
// of the provided |service| to the provided |callback|.
class ProviderCheckConsumer : public WebIntentsRegistry::Consumer {
 public:
  ProviderCheckConsumer(const WebIntentServiceData& service,
                        const base::Callback<void(bool)>& callback)
      : callback_(callback),
        service_(service) {}
  virtual ~ProviderCheckConsumer() {}

  // Gets the list of all providers for a particular action. Check them all
  // to see if |provider_| is already registered.
  virtual void OnIntentsQueryDone(
      WebIntentsRegistry::QueryID id,
      const WebIntentsRegistry::IntentServiceList& list) OVERRIDE {
    scoped_ptr<ProviderCheckConsumer> self_deleter(this);

    for (WebIntentsRegistry::IntentServiceList::const_iterator i = list.begin();
         i != list.end(); ++i) {
      if (*i == service_) {
        callback_.Run(true);
        return;
      }
    }

    callback_.Run(false);
  }

 private:
  base::Callback<void(bool)> callback_;
  WebIntentServiceData service_;
};

WebIntentsRegistry::QueryID WebIntentsRegistry::IntentProviderExists(
    const WebIntentServiceData& provider,
    const base::Callback<void(bool)>& callback) {
  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->type_ = ASCIIToUTF16("*");
  query->consumer_ = new ProviderCheckConsumer(provider, callback);
  query->pending_query_ = wds_->GetWebIntentServicesForURL(
      UTF8ToUTF16(provider.service_url.spec()), this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

void WebIntentsRegistry::RegisterIntentProvider(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->AddWebIntentService(service);
}

void WebIntentsRegistry::UnregisterIntentProvider(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->RemoveWebIntentService(service);
}
