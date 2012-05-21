// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
#pragma once

#include "base/callback_forward.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "webkit/glue/web_intent_service_data.h"

struct DefaultWebIntentService;

namespace extensions {
class Extension;
}

// Handles storing and retrieving of web intents services in the web database.
// The registry provides filtering logic to retrieve specific types of services.
class WebIntentsRegistry
    : public WebDataServiceConsumer,
      public ProfileKeyedService {
 public:
  typedef std::vector<webkit_glue::WebIntentServiceData> IntentServiceList;

  // Callback used by WebIntentsRegistry to return results of data fetch.
  typedef base::Callback<void(const IntentServiceList&)>
      QueryCallback;

  // Callback to return results of a defaults query.
  typedef base::Callback<void(const DefaultWebIntentService&)>
      DefaultQueryCallback;

  // Initializes, binds to a valid WebDataService.
  void Initialize(scoped_refptr<WebDataService> wds,
                  ExtensionServiceInterface* extension_service);

  // Registers a service.
  virtual void RegisterIntentService(
      const webkit_glue::WebIntentServiceData& service);

  // Removes a service from the registry.
  void UnregisterIntentService(
      const webkit_glue::WebIntentServiceData& service);

  // Requests all services matching |action| and |mimetype|.
  // |mimetype| can contain wildcards, i.e. "image/*" or "*".
  // |callback| must not be null.
  void GetIntentServices(const string16& action,
                         const string16& mimetype,
                         const QueryCallback& callback);

  // Requests all services.
  // |callback| must not be null.
  void GetAllIntentServices(const QueryCallback& callback);

  // Tests for the existence of the given |service|. Calls the
  // provided |callback| with true if it exists, false if it does not.
  // Checks for |service| equality with ==.
  // |callback| must not be null.
  void IntentServiceExists(
      const webkit_glue::WebIntentServiceData& service,
      const base::Callback<void(bool)>& callback);

  // Requests all extension services matching |action|, |mimetype| and
  // |extension_id|.
  // |mimetype| must conform to definition as outlined for GetIntentServices.
  // |callback| must not be null.
  void GetIntentServicesForExtensionFilter(const string16& action,
                                              const string16& mimetype,
                                              const std::string& extension_id,
                                              const QueryCallback& callback);

  // Record the given default service entry.
  virtual void RegisterDefaultIntentService(
      const DefaultWebIntentService& default_service);

  // Delete the given default service entry. Deletes entries matching
  // the |action|, |type|, and |url_pattern| of |default_service|.
  virtual void UnregisterDefaultIntentService(
      const DefaultWebIntentService& default_service);

  // Requests the best default intent service for the given invocation
  // parameters.
  // |callback| must not be null.
  void GetDefaultIntentService(const string16& action,
                                  const string16& type,
                                  const GURL& invoking_url,
                                  const DefaultQueryCallback& callback);

 protected:
  // Make sure that only WebIntentsRegistryFactory can create an instance of
  // WebIntentsRegistry.
  friend class WebIntentsRegistryFactory;
  friend class WebIntentsRegistryTest;
  friend class WebIntentsModelTest;
  FRIEND_TEST_ALL_PREFIXES(WebIntentsRegistryTest, CollapseIntents);

  WebIntentsRegistry();
  virtual ~WebIntentsRegistry();

  // Collapses a list of IntentServices by joining intents that have identical
  // service URLs, actions, and mime types.
  // |services| is the list of intent services to be collapsed when passed in
  // and will be modified with the new list in-place.
  void CollapseIntents(IntentServiceList* services);

 private:
   const extensions::Extension* ExtensionForURL(const std::string& url);

   struct IntentsQuery;

  // Maps web data requests to intents queries.
  // Allows OnWebDataServiceRequestDone to forward to appropriate consumer.
  typedef base::hash_map<WebDataService::Handle, IntentsQuery*> QueryMap;

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result) OVERRIDE;

  // Delegate for defaults requests from OnWebDataServiceRequestDone.
  virtual void OnWebDataServiceDefaultsRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result);

  // Implementation of GetIntentServicesForExtensionFilter.
  void DoGetIntentServicesForExtensionFilter(scoped_ptr<IntentsQuery> query,
                                             const std::string& extension_id);

  // Map for all in-flight web data requests/intent queries.
  QueryMap queries_;

  // Local reference to Web Data Service.
  scoped_refptr<WebDataService> wds_;

  // Local reference to the ExtensionService.
  // Shutdown/cleanup is handled by ProfileImpl. We are  guaranteed that any
  // ProfileKeyedService will be shut down before data on ProfileImpl is
  // destroyed (i.e. |extension_service_|), so |extension_service_| is valid
  // for the lifetime of the WebIntentsRegistry object.
  ExtensionServiceInterface* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsRegistry);
};

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
