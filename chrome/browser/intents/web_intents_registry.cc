// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"

namespace {

typedef WebIntentsRegistry::IntentServiceList IntentServiceList;

// Compares two mime types for equality. Supports wild cards in both
// |type1| and |type2|. Wild cards are of the form '<type>/*' or '*'.
bool MimeTypesAreEqual(const string16& type1, const string16& type2) {
  // We don't have a MIME matcher that allows patterns on both sides
  // Instead, we do two comparisons, treating each type in turn as a
  // pattern. If either one matches, we consider this a MIME match.
  if (net::MatchesMimeType(UTF16ToUTF8(type1), UTF16ToUTF8(type2)))
    return true;
  return net::MatchesMimeType(UTF16ToUTF8(type2), UTF16ToUTF8(type1));
}

// Adds any intent services of |extension| that match |action| to
// |matching_services|.
void AddMatchingServicesForExtension(const Extension& extension,
                                     const string16& action,
                                     IntentServiceList* matching_services) {
  const IntentServiceList& services = extension.intents_services();
  for (IntentServiceList::const_iterator i = services.begin();
       i != services.end(); ++i) {
    if (action.empty() || action == i->action)
      matching_services->push_back(*i);
  }
}

// Removes all services from |matching_services| that do not match |mimetype|.
// Wildcards are supported, of the form '<type>/*' or '*'.
void FilterServicesByMimetype(const string16& mimetype,
                              IntentServiceList* matching_services) {
  // Filter out all services not matching the query type.
  IntentServiceList::iterator iter(matching_services->begin());
  while (iter != matching_services->end()) {
    if (MimeTypesAreEqual(iter->type, mimetype))
      ++iter;
    else
      iter = matching_services->erase(iter);
  }
}

// Functor object for intent ordering.
struct IntentOrdering {
  // Implements StrictWeakOrdering for intents, based on intent-equivalence.
  // Order by |service_url|, |action|, |title|, and |disposition| in order.
  bool operator()(const webkit_glue::WebIntentServiceData& lhs,
                  const webkit_glue::WebIntentServiceData& rhs) {
    if (lhs.service_url != rhs.service_url)
      return lhs.service_url < rhs.service_url;

    if (lhs.action != rhs.action)
      return lhs.action < rhs.action;

    if (lhs.title != rhs.title)
      return lhs.title < rhs.title;

    if (lhs.disposition != rhs.disposition)
      return lhs.disposition < rhs.disposition;

    // At this point, we consider intents to be equal, even if |type| differs.
    return false;
  }
};

// Two intents are equivalent iff all fields except |type| are equal.
bool IntentsAreEquivalent(const webkit_glue::WebIntentServiceData& lhs,
                          const webkit_glue::WebIntentServiceData& rhs) {
  return !((lhs.service_url != rhs.service_url) ||
      (lhs.action != rhs.action) ||
      (lhs.title != rhs.title) ||
      (lhs.disposition != rhs.disposition));
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
  // If |action_| is empty, return all extension-provided services.
  string16 action_;

  // The MIME type that was requested for this service query.
  // Suppports wild cards.
  string16 type_;

  // The url of the invoking page.
  GURL url_;

  // Create a new IntentsQuery for services with the specified action/type.
  IntentsQuery(QueryID id, Consumer* consumer,
               const string16& action, const string16& type)
      : query_id_(id), consumer_(consumer), action_(action), type_(type) {}

  // Create a new IntentsQuery for all intent services or for existence checks.
  IntentsQuery(QueryID id, Consumer* consumer)
      : query_id_(id), consumer_(consumer), type_(ASCIIToUTF16("*")) {}

  // Create a new IntentsQuery for default services.
  IntentsQuery(QueryID id, Consumer* consumer,
               const string16& action, const string16& type, const GURL& url)
      : query_id_(id), consumer_(consumer),
        action_(action), type_(type), url_(url) {}
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
  if (result->GetType() == WEB_INTENTS_DEFAULTS_RESULT) {
    OnWebDataServiceDefaultsRequestDone(h, result);
    return;
  }
  DCHECK(result->GetType() == WEB_INTENTS_RESULT);

  QueryMap::iterator it = queries_.find(h);
  DCHECK(it != queries_.end());

  IntentsQuery* query(it->second);
  DCHECK(query);
  queries_.erase(it);

  IntentServiceList matching_services = static_cast<
      const WDResult<IntentServiceList>*>(result)->GetValue();

  // Loop over all services in all extensions, collect ones
  // matching the query.
  if (extension_service_) {
    const ExtensionSet* extensions = extension_service_->extensions();
    if (extensions) {
      for (ExtensionSet::const_iterator i(extensions->begin());
           i != extensions->end(); ++i) {
        AddMatchingServicesForExtension(**i, query->action_,
                                        &matching_services);
      }
    }
  }

  // Filter out all services not matching the query type.
  FilterServicesByMimetype(query->type_, &matching_services);

  // Collapse intents that are equivalent for all but |type|.
  CollapseIntents(&matching_services);

  query->consumer_->OnIntentsQueryDone(query->query_id_, matching_services);
  delete query;
}

const Extension* WebIntentsRegistry::ExtensionForURL(const std::string& url) {
  const ExtensionSet* extensions = extension_service_->extensions();
  if (!extensions)
    return NULL;

  // Use the unsafe ExtensionURLInfo constructor: we don't care if the extension
  // is running or not.
  GURL gurl(url);
  ExtensionURLInfo info(gurl);
  return extensions->GetExtensionOrAppByURL(info);
}

void WebIntentsRegistry::OnWebDataServiceDefaultsRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  QueryMap::iterator it = queries_.find(h);
  DCHECK(it != queries_.end());

  IntentsQuery* query(it->second);
  DCHECK(query);
  queries_.erase(it);

  std::vector<DefaultWebIntentService> services = static_cast<
      const WDResult<std::vector<DefaultWebIntentService> >*>(result)->
          GetValue();

  DefaultWebIntentService default_service;
  std::vector<DefaultWebIntentService>::iterator iter(services.begin());
  for (; iter != services.end(); ++iter) {
    if (!MimeTypesAreEqual(iter->type, query->type_)) {
      continue;
    }
    if (!iter->url_pattern.MatchesURL(query->url_)) {
      continue;
    }
    const Extension* extension = ExtensionForURL(iter->service_url);
    if (extension != NULL &&
        !extension_service_->IsExtensionEnabled(extension->id())) {
      continue;
    }

    // Found a match. If it is better than default_service, use it.
    // Currently the metric is that if the new value is user-set,
    // prefer it. If the present value is suppressed, prefer it.
    // If the new value has a more specific pattern, prefer it.
    if (default_service.user_date <= 0 && iter->user_date >= 0)
      default_service = *iter;
    else if (default_service.suppression > 0 && iter->suppression <= 0)
      default_service = *iter;
    else if (default_service.url_pattern.match_all_urls() &&
             !iter->url_pattern.match_all_urls())
      default_service = *iter;
    else if (iter->url_pattern < default_service.url_pattern)
      default_service = *iter;
  }

  query->consumer_->OnIntentsDefaultsQueryDone(query->query_id_,
                                               default_service);
  delete query;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetIntentServices(
    const string16& action, const string16& mimetype, Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query =
      new IntentsQuery(next_query_id_++, consumer, action, mimetype);
  query->pending_query_ = wds_->GetWebIntentServices(action, this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetAllIntentServices(
    Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery(next_query_id_++, consumer);
  query->pending_query_ = wds_->GetAllWebIntentServices(this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

// Trampoline consumer for calls to IntentServiceExists. Forwards existence
// of the provided |service| to the provided |callback|.
class ServiceCheckConsumer : public WebIntentsRegistry::Consumer {
 public:
  ServiceCheckConsumer(const WebIntentServiceData& service,
                       const base::Callback<void(bool)>& callback)
      : callback_(callback),
        service_(service) {}
  virtual ~ServiceCheckConsumer() {}

  // Gets the list of all services for a particular action. Check them all
  // to see if |service_| is already registered.
  virtual void OnIntentsQueryDone(
      WebIntentsRegistry::QueryID id,
      const WebIntentsRegistry::IntentServiceList& list) OVERRIDE {
    scoped_ptr<ServiceCheckConsumer> self_deleter(this);

    for (WebIntentsRegistry::IntentServiceList::const_iterator i = list.begin();
         i != list.end(); ++i) {
      if (*i == service_) {
        callback_.Run(true);
        return;
      }
    }

    callback_.Run(false);
  }

  virtual void OnIntentsDefaultsQueryDone(
      WebIntentsRegistry::QueryID query_id,
      const DefaultWebIntentService& default_service) {}

 private:
  base::Callback<void(bool)> callback_;
  WebIntentServiceData service_;
};

WebIntentsRegistry::QueryID WebIntentsRegistry::IntentServiceExists(
    const WebIntentServiceData& service,
    const base::Callback<void(bool)>& callback) {
  IntentsQuery* query = new IntentsQuery(
      next_query_id_++, new ServiceCheckConsumer(service, callback));
  query->pending_query_ = wds_->GetWebIntentServicesForURL(
      UTF8ToUTF16(service.service_url.spec()), this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

WebIntentsRegistry::QueryID
    WebIntentsRegistry::GetIntentServicesForExtensionFilter(
        const string16& action,
        const string16& mimetype,
        const std::string& extension_id,
        Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<IntentsQuery> query(
      new IntentsQuery(next_query_id_++, consumer, action, mimetype));
  int query_id = query->query_id_;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebIntentsRegistry::DoGetIntentServicesForExtensionFilter,
                 base::Unretained(this),
                 base::Passed(&query), extension_id));

  return query_id;
}

void WebIntentsRegistry::DoGetIntentServicesForExtensionFilter(
    scoped_ptr<IntentsQuery> query,
    const std::string& extension_id) {
  IntentServiceList matching_services;

  if (extension_service_) {
    const Extension* extension =
        extension_service_->GetExtensionById(extension_id, false);
    AddMatchingServicesForExtension(*extension,
                                    query->action_,
                                    &matching_services);
    FilterServicesByMimetype(query->type_, &matching_services);
  }

  query->consumer_->OnIntentsQueryDone(query->query_id_, matching_services);
}

void WebIntentsRegistry::RegisterDefaultIntentService(
    const DefaultWebIntentService& default_service) {
  DCHECK(wds_.get());
  wds_->AddDefaultWebIntentService(default_service);
}

void WebIntentsRegistry::UnregisterDefaultIntentService(
    const DefaultWebIntentService& default_service) {
  DCHECK(wds_.get());
  wds_->RemoveDefaultWebIntentService(default_service);
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetDefaultIntentService(
    const string16& action,
    const string16& type,
    const GURL& invoking_url,
    Consumer* consumer) {
  IntentsQuery* query =
      new IntentsQuery(next_query_id_++, consumer, action, type, invoking_url);
  query->pending_query_ =
      wds_->GetDefaultWebIntentServicesForAction(action, this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

void WebIntentsRegistry::RegisterIntentService(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->AddWebIntentService(service);
}

void WebIntentsRegistry::UnregisterIntentService(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->RemoveWebIntentService(service);
}

void WebIntentsRegistry::CollapseIntents(IntentServiceList* services) {
  DCHECK(services);

  // No need to do anything for no services/single service.
  if (services->size() < 2)
    return;

  // Sort so that intents that can be collapsed must be adjacent.
  std::sort(services->begin(), services->end(), IntentOrdering());

  // Combine adjacent services if they are equivalent.
  IntentServiceList::iterator write_iter = services->begin();
  IntentServiceList::iterator read_iter = write_iter + 1;
  while (read_iter != services->end()) {
    if (IntentsAreEquivalent(*write_iter, *read_iter)) {
      // If the two intents are equivalent, join types and collapse.
      write_iter->type += ASCIIToUTF16(",") + read_iter->type;
    } else {
      // Otherwise, keep both intents.
      ++write_iter;
      if (write_iter != read_iter)
        *write_iter = *read_iter;
    }
    ++read_iter;
  }

  // Cut off everything after the last intent copied to the list.
  if (++write_iter != services->end())
    services->erase(write_iter, services->end());
}
