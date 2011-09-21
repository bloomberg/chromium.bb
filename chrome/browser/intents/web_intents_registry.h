// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
#pragma once

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/intents/web_intent_service_data.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/webdata/web_data_service.h"

// Handles storing and retrieving of web intents in the web database.
// The registry provides filtering logic to retrieve specific types of intents.
class WebIntentsRegistry
    : public WebDataServiceConsumer,
      public ProfileKeyedService {
 public:
  // Unique identifier for intent queries.
  typedef int QueryID;

  // An interface the WebIntentsRegistry uses to notify its clients when
  // it has finished loading intents data from the web database.
  class Consumer {
   public:
    // Notifies the observer that the intents request has been completed.
    virtual void OnIntentsQueryDone(
        QueryID query_id,
        const std::vector<WebIntentServiceData>& intents) = 0;

   protected:
    virtual ~Consumer() {}
  };

  // Initializes, binds to a valid WebDataService.
  void Initialize(scoped_refptr<WebDataService> wds);

  // Registers a web intent provider.
  virtual void RegisterIntentProvider(const WebIntentServiceData& intent);

  // Removes a web intent provider from the registry.
  void UnregisterIntentProvider(const WebIntentServiceData& intent);

  // Requests all intent providers matching |action|.
  // |consumer| must not be NULL.
  QueryID GetIntentProviders(const string16& action, Consumer* consumer);

  // Requests all intent providers. |consumer| must not be NULL
  QueryID GetAllIntentProviders(Consumer* consumer);

 protected:
  // Make sure that only WebIntentsRegistryFactory can create an instance of
  // WebIntentsRegistry.
  friend class WebIntentsRegistryFactory;
  friend class WebIntentsRegistryTest;
  friend class WebIntentsModelTest;

  WebIntentsRegistry();
  virtual ~WebIntentsRegistry();

 private:
   struct IntentsQuery;

  // Maps web data requests to intents queries.
  // Allows OnWebDataServiceRequestDone to forward to appropriate consumer.
  typedef base::hash_map<WebDataService::Handle, IntentsQuery*> QueryMap;

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Map for all in-flight web data requests/intent queries.
  QueryMap queries_;

  // Unique identifier for next intent query.
  QueryID next_query_id_;

  // Local reference to Web Data Service.
  scoped_refptr<WebDataService> wds_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsRegistry);
};

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_H_
