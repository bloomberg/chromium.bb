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
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/web_intents_handler.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"

using extensions::Extension;
using net::IsMimeType;

namespace {

// TODO(hshi): Temporary workaround for http://crbug.com/134197.
// If no user-set default service is found, use built-in QuickOffice Viewer as
// default for MS office files. Remove this once full defaults is in place.
const char* kQuickOfficeViewerMimeType[] = {
  "application/msword",
  "application/vnd.ms-powerpoint",
  "application/vnd.ms-excel",
  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
};

typedef base::Callback<void(const WDTypedResult* result)> ResultsHandler;
typedef WebIntentsRegistry::IntentServiceList IntentServiceList;

// Compares two web intents type specifiers to see if there is a match.
// First checks if both are MIME types. If so, uses MatchesMimeType.
// If not, uses exact string equality.
bool WebIntentsTypesMatch(const string16& type1, const string16& type2) {
  return (IsMimeType(UTF16ToUTF8(type1)) && IsMimeType(UTF16ToUTF8(type2)))
      ? web_intents::MimeTypesMatch(type1, type2)
      : type1 == type2;
}

// Adds any intent services of |extension| that match |action| to
// |matching_services|.
void AddMatchingServicesForExtension(const Extension& extension,
                                     const string16& action,
                                     IntentServiceList* matching_services) {
  const IntentServiceList& services =
      extensions::WebIntentsInfo::GetIntentsServices(&extension);
  for (IntentServiceList::const_iterator i = services.begin();
       i != services.end(); ++i) {
    if (action.empty() || action == i->action)
      matching_services->push_back(*i);
  }
}

// Removes all services from |matching_services| that do not match |type|.
// Wildcards are supported, of the form '<type>/*' or '*'.
void FilterServicesByType(const string16& type,
                          IntentServiceList* matching_services) {
  // Filter out all services not matching the query type.
  IntentServiceList::iterator iter(matching_services->begin());
  while (iter != matching_services->end()) {
    if (WebIntentsTypesMatch(iter->type, type))
      ++iter;
    else
      iter = matching_services->erase(iter);
  }
}

// Callback for existence checks. Converts a callback for a list of services
// into a callback that returns true if the list contains a specific service.
void ExistenceCallback(const webkit_glue::WebIntentServiceData& service,
                       const base::Callback<void(bool)>& callback,
                       const WDTypedResult* result) {

  WebIntentsRegistry::IntentServiceList list = static_cast<
      const WDResult<IntentServiceList>*>(result)->GetValue();

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

// Internal object containing arguments to be used in post processing
// WDS results.
struct WebIntentsRegistry::QueryParams {

  // The particular action to filter for while searching through extensions.
  // If |action_| is empty, return all extension-provided services.
  string16 action_;

  // The MIME type that was requested for this service query.
  // Suppports wild cards.
  string16 type_;

  // The url of the invoking page.
  GURL url_;

  // Create a new QueryParams for all intent services or for existence checks.
  QueryParams() : type_(ASCIIToUTF16("*")) {}

  QueryParams(const string16& action, const string16& type)
      : action_(action), type_(type) {}

  // Create a new QueryParams for default services.
  QueryParams(const string16& action, const string16& type, const GURL& url)
      : action_(action), type_(type), url_(url) {}
};

// Internal object adapting the WDS consumer interface to base::Bind
// callback way of doing business.
class WebIntentsRegistry::QueryAdapter : public WebDataServiceConsumer {

 public:
  // Underlying data query.
  WebDataService::Handle query_handle_;

  // Create a new QueryAdapter that delegates results to |handler|.
  QueryAdapter(WebIntentsRegistry* registry, const ResultsHandler& handler)
      : registry_(registry), handler_(handler) {
    registry_->TrackQuery(this);
  }

  void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result) OVERRIDE {

    handler_.Run(result);
    registry_->ReleaseQuery(this);
  }

 private:
  // Handle so we can call back into the WebIntentsRegistry when
  // processing query results. The registry is guaranteed to be
  // valid for the life of this object. We do not own this object.
  WebIntentsRegistry* registry_;

  // The callback for this particular query.
  ResultsHandler handler_;
};

WebIntentsRegistry::WebIntentsRegistry() {
  native_services_.reset(new web_intents::NativeServiceRegistry());
}

WebIntentsRegistry::~WebIntentsRegistry() {

  // Cancel all pending queries, since we can't handle them any more.
  for (QueryVector::iterator it = pending_queries_.begin();
      it != pending_queries_.end(); ++it) {
    QueryAdapter* query = *it;
    wds_->CancelRequest(query->query_handle_);
    delete query;
  }
}

void WebIntentsRegistry::Initialize(
    scoped_refptr<WebDataService> wds,
    ExtensionServiceInterface* extension_service) {
  wds_ = wds;
  extension_service_ = extension_service;
}

void WebIntentsRegistry::OnWebIntentsResultReceived(
    const QueryParams& params,
    const QueryCallback& callback,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_RESULT);

  IntentServiceList matching_services = static_cast<
      const WDResult<IntentServiceList>*>(result)->GetValue();

  // Loop over all services in all extensions, collect ones
  // matching the query.
  if (extension_service_) {
    const ExtensionSet* extensions = extension_service_->extensions();
    if (extensions) {
      for (ExtensionSet::const_iterator i(extensions->begin());
           i != extensions->end(); ++i) {
        AddMatchingServicesForExtension(**i, params.action_,
                                        &matching_services);
      }
    }
  }

  // add native services.
  native_services_->GetSupportedServices(params.action_, &matching_services);

  // Filter out all services not matching the query type.
  FilterServicesByType(params.type_, &matching_services);

  // Collapse intents that are equivalent for all but |type|.
  CollapseIntents(&matching_services);

  callback.Run(matching_services);
}

void WebIntentsRegistry::OnAllDefaultIntentServicesReceived(
    const DefaultIntentServicesCallback& callback,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_DEFAULTS_RESULT);

  const std::vector<DefaultWebIntentService> services = static_cast<
      const WDResult<std::vector<DefaultWebIntentService> >*>(result)->
          GetValue();

  callback.Run(services);
}

void WebIntentsRegistry::OnWebIntentsDefaultsResultReceived(
    const QueryParams& params,
    const DefaultQueryCallback& callback,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_DEFAULTS_RESULT);

  std::vector<DefaultWebIntentService> services = static_cast<
      const WDResult<std::vector<DefaultWebIntentService> >*>(result)->
          GetValue();

  DefaultWebIntentService default_service;
  std::vector<DefaultWebIntentService>::iterator iter(services.begin());
  for (; iter != services.end(); ++iter) {
    if (!WebIntentsTypesMatch(iter->type, params.type_)) {
      continue;
    }
    if (!iter->url_pattern.MatchesURL(params.url_)) {
      continue;
    }
    const Extension* extension = ExtensionForURL(iter->service_url);
    if (extension != NULL &&
        !extension_service_->IsExtensionEnabled(extension->id())) {
      continue;
    }

    // Found a match. If it is better than default_service, use it.
    // Currently the metric is that if the new value is user-set,
    // prefer it. If the new value has a more specific pattern, prefer it.
    // If the new value has a more recent date, prefer it.
    if (default_service.user_date <= 0 && iter->user_date >= 0) {
      default_service = *iter;
    } else if (default_service.url_pattern.match_all_urls() &&
             !iter->url_pattern.match_all_urls()) {
      default_service = *iter;
    } else if (iter->url_pattern < default_service.url_pattern) {
      default_service = *iter;
    } else if (default_service.user_date < iter->user_date) {
      default_service = *iter;
    }
  }

  // If QuickOffice is the default for this, recompute defaults.
  if (default_service.service_url
      == web_intents::kQuickOfficeViewerServiceURL ||
      default_service.service_url
      == web_intents::kQuickOfficeViewerDevServiceURL) {
    default_service.user_date = -1;
  }

  // TODO(hshi): Temporary workaround for http://crbug.com/134197.
  // If no user-set default service is found, use built-in QuickOffice Viewer as
  // default for MS office files. Remove this once full defaults is in place.

  if (default_service.user_date <= 0) {
    const char kQuickOfficeDevExtensionId[] =
        "ionpfmkccalenbmnddpbmocokhaknphg";
    std::string service_url = web_intents::kQuickOfficeViewerServiceURL;
    if (extension_service_->GetInstalledExtension(kQuickOfficeDevExtensionId) &&
        extension_service_->IsExtensionEnabled(kQuickOfficeDevExtensionId)) {
        service_url = web_intents::kQuickOfficeViewerDevServiceURL;
    }

    for (size_t i = 0; i < sizeof(kQuickOfficeViewerMimeType) / sizeof(char*);
         ++i) {
      DefaultWebIntentService qoviewer_service;
      qoviewer_service.action = ASCIIToUTF16(web_intents::kActionView);
      qoviewer_service.type = ASCIIToUTF16(kQuickOfficeViewerMimeType[i]);
      qoviewer_service.service_url = service_url;
      if (WebIntentsTypesMatch(qoviewer_service.type, params.type_)) {
        default_service = qoviewer_service;
        break;
      }
    }
  }

  callback.Run(default_service);
}

void WebIntentsRegistry::GetIntentServices(
    const string16& action, const string16& type,
    const QueryCallback& callback) {
  DCHECK(wds_.get());
  DCHECK(!callback.is_null());

  const QueryParams params(action, type);
  const ResultsHandler handler = base::Bind(
      &WebIntentsRegistry::OnWebIntentsResultReceived,
      base::Unretained(this),
      params,
      callback);

  QueryAdapter* query = new QueryAdapter(this, handler);
  query->query_handle_ = wds_->GetWebIntentServicesForAction(action, query);
}

void WebIntentsRegistry::GetAllIntentServices(
    const QueryCallback& callback) {
  DCHECK(wds_.get());
  DCHECK(!callback.is_null());

  const QueryParams params;
  const ResultsHandler handler = base::Bind(
      &WebIntentsRegistry::OnWebIntentsResultReceived,
      base::Unretained(this),
      params,
      callback);

  QueryAdapter* query = new QueryAdapter(this, handler);
  query->query_handle_ = wds_->GetAllWebIntentServices(query);
}

void WebIntentsRegistry::GetAllDefaultIntentServices(
    const DefaultIntentServicesCallback& callback) {
  DCHECK(!callback.is_null());

  ResultsHandler handler = base::Bind(
      &WebIntentsRegistry::OnAllDefaultIntentServicesReceived,
      base::Unretained(this),
      callback);

  QueryAdapter* query = new QueryAdapter(this, handler);
  query->query_handle_ =
      wds_->GetAllDefaultWebIntentServices(query);
}

void WebIntentsRegistry::IntentServiceExists(
    const WebIntentServiceData& service,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!callback.is_null());

  ResultsHandler handler = base::Bind(
      &ExistenceCallback,
      service,
      callback);

  QueryAdapter* query = new QueryAdapter(this, handler);
  query->query_handle_ = wds_->GetWebIntentServicesForURL(
      UTF8ToUTF16(service.service_url.spec()), query);
}

void WebIntentsRegistry::GetIntentServicesForExtensionFilter(
        const string16& action,
        const string16& type,
        const std::string& extension_id,
        IntentServiceList* services) {
  if (extension_service_) {
    const Extension* extension =
        extension_service_->GetExtensionById(extension_id, false);
    AddMatchingServicesForExtension(*extension,
                                    action,
                                    services);
    FilterServicesByType(type, services);
  }
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

void WebIntentsRegistry::UnregisterServiceDefaults(const GURL& service_url) {
  DCHECK(wds_.get());
  wds_->RemoveWebIntentServiceDefaults(service_url);
}

void WebIntentsRegistry::GetDefaultIntentService(
    const string16& action,
    const string16& type,
    const GURL& invoking_url,
    const DefaultQueryCallback& callback) {
  DCHECK(!callback.is_null());

  const QueryParams params(action, type);

  ResultsHandler handler = base::Bind(
      &WebIntentsRegistry::OnWebIntentsDefaultsResultReceived,
      base::Unretained(this),
      params,
      callback);

  QueryAdapter* query = new QueryAdapter(this, handler);
  query->query_handle_ =
      wds_->GetDefaultWebIntentServicesForAction(action, query);
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

void WebIntentsRegistry::TrackQuery(QueryAdapter* query) {
  DCHECK(query);
  pending_queries_.push_back(query);
}

void WebIntentsRegistry::ReleaseQuery(QueryAdapter* query) {
  QueryVector::iterator it = std::find(
      pending_queries_.begin(), pending_queries_.end(), query);
  if (it != pending_queries_.end()) {
    pending_queries_.erase(it);
    delete query;
  } else {
    NOTREACHED();
  }
}
