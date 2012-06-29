// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"

using extensions::Extension;

namespace {

// TODO(hshi): Temporary workaround for http://crbug.com/134197.
// If no user-set default service is found, use built-in QuickOffice Viewer as
// default for MS office files. Remove this once full defaults is in place.
const char kViewActionURL[] = "http://webintents.org/view";

const char kQuickOfficeViewerServiceURL[] =
  "chrome-extension://gbkeegbaiigmenfmjfclcdgdpimamgkj/views/appViewer.html";

const char* kQuickOfficeViewerMimeType[] = {
  "application/msword",
  "application/vnd.ms-powerpoint",
  "application/vnd.ms-excel",
  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
};

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

// Returns true if the passed string is a MIME type. Works by comparing string
// prefix to the valid MIME top-level types (and the wildcard type */).
// "*" is also accepted as a valid MIME type.
// The passed |type_str| should have no leading or trailing whitespace.
bool IsMimeType(const string16& type_str) {
  return net::IsMimeType(UTF16ToUTF8(type_str));
}

// Compares two web intents type specifiers to see if there is a match.
// First checks if both are MIME types. If so, uses MatchesMimeType.
// If not, uses exact string equality.
bool WebIntentsTypesMatch(const string16& type1, const string16& type2) {
  if (IsMimeType(type1) && IsMimeType(type2))
    return MimeTypesAreEqual(type1, type2);

  return type1 == type2;
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
    if (WebIntentsTypesMatch(iter->type, mimetype))
      ++iter;
    else
      iter = matching_services->erase(iter);
  }
}

// Callback for existence checks. Converts a callback for a list of services
// into a callback that returns true if the list contains a specific service.
void ExistenceCallback(const webkit_glue::WebIntentServiceData& service,
                       const base::Callback<void(bool)>& callback,
                       const WebIntentsRegistry::IntentServiceList& list) {
  for (WebIntentsRegistry::IntentServiceList::const_iterator i = list.begin();
       i != list.end(); ++i) {
    if (*i == service) {
      callback.Run(true);
      return;
    }
  }

  callback.Run(false);
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
  // Underlying data query.
  WebDataService::Handle pending_query_;

  // The callback for this particular query.
  QueryCallback callback_;

  // Callback for a query for defaults.
  DefaultQueryCallback default_callback_;

  // The particular action to filter for while searching through extensions.
  // If |action_| is empty, return all extension-provided services.
  string16 action_;

  // The MIME type that was requested for this service query.
  // Suppports wild cards.
  string16 type_;

  // The url of the invoking page.
  GURL url_;

  // Create a new IntentsQuery for services with the specified action/type.
  IntentsQuery(const QueryCallback& callback,
               const string16& action, const string16& type)
      : callback_(callback), action_(action), type_(type) {}

  // Create a new IntentsQuery for all intent services or for existence checks.
  explicit IntentsQuery(const QueryCallback callback)
      : callback_(callback), type_(ASCIIToUTF16("*")) {}

  // Create a new IntentsQuery for default services.
  IntentsQuery(const DefaultQueryCallback& callback,
               const string16& action, const string16& type, const GURL& url)
      : default_callback_(callback), action_(action), type_(type), url_(url) {}
};

WebIntentsRegistry::WebIntentsRegistry() {}

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

  query->callback_.Run(matching_services);
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
    if (!WebIntentsTypesMatch(iter->type, query->type_)) {
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

  // TODO(hshi): Temporary workaround for http://crbug.com/134197.
  // If no user-set default service is found, use built-in QuickOffice Viewer as
  // default for MS office files. Remove this once full defaults is in place.
  if (default_service.user_date <= 0) {
    for (size_t i = 0; i < sizeof(kQuickOfficeViewerMimeType) / sizeof(char*);
         ++i) {
      DefaultWebIntentService qoviewer_service;
      qoviewer_service.action = ASCIIToUTF16(kViewActionURL);
      qoviewer_service.type = ASCIIToUTF16(kQuickOfficeViewerMimeType[i]);
      qoviewer_service.service_url = kQuickOfficeViewerServiceURL;
      if (WebIntentsTypesMatch(qoviewer_service.type, query->type_)) {
        default_service = qoviewer_service;
        break;
      }
    }
  }

  query->default_callback_.Run(default_service);
  delete query;
}

void WebIntentsRegistry::GetIntentServices(
    const string16& action, const string16& mimetype,
    const QueryCallback& callback) {
  DCHECK(wds_.get());
  DCHECK(!callback.is_null());

  IntentsQuery* query = new IntentsQuery(callback, action, mimetype);
  query->pending_query_ = wds_->GetWebIntentServices(action, this);
  queries_[query->pending_query_] = query;
}

void WebIntentsRegistry::GetAllIntentServices(
    const QueryCallback& callback) {
  DCHECK(wds_.get());
  DCHECK(!callback.is_null());

  IntentsQuery* query = new IntentsQuery(callback);
  query->pending_query_ = wds_->GetAllWebIntentServices(this);
  queries_[query->pending_query_] = query;
}

void WebIntentsRegistry::IntentServiceExists(
    const WebIntentServiceData& service,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!callback.is_null());

  IntentsQuery* query = new IntentsQuery(
      base::Bind(&ExistenceCallback, service, callback));
  query->pending_query_ = wds_->GetWebIntentServicesForURL(
      UTF8ToUTF16(service.service_url.spec()), this);
  queries_[query->pending_query_] = query;
}

void WebIntentsRegistry::GetIntentServicesForExtensionFilter(
        const string16& action,
        const string16& mimetype,
        const std::string& extension_id,
        const QueryCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<IntentsQuery> query(
      new IntentsQuery(callback, action, mimetype));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebIntentsRegistry::DoGetIntentServicesForExtensionFilter,
                 base::Unretained(this),
                 base::Passed(&query), extension_id));
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

  query->callback_.Run(matching_services);
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

void WebIntentsRegistry::GetDefaultIntentService(
    const string16& action,
    const string16& type,
    const GURL& invoking_url,
    const DefaultQueryCallback& callback) {
  DCHECK(!callback.is_null());

  IntentsQuery* query =
      new IntentsQuery(callback, action, type, invoking_url);
  query->pending_query_ =
      wds_->GetDefaultWebIntentServicesForAction(action, this);
  queries_[query->pending_query_] = query;
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
