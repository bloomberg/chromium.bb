// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_H_
#define CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_H_
#pragma once

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace net {
class URLRequestContextGetter;
}

// Handles storing and retrieving of web intents in the web database.
// The registry provides filtering logic to retrieve specific types of intents.
class CWSIntentsRegistry : public ProfileKeyedService,
                           public content::URLFetcherDelegate {
 public:
  // Data returned from CWS for a single intent.
  struct IntentExtensionInfo {
    int num_ratings;  // Number of ratings in CWS store.
    double average_rating;  // The average CWS rating.
    string16 manifest;  // The containing extension's manifest info.
    GURL icon_url;  // Where to retrieve an icon for this intent.
  };

  // List of Intent extensions, as returned by GetIntentProviders's |callback|
  typedef std::vector<IntentExtensionInfo> IntentExtensionList;
  // Callback to return results from GetIntentProviders upon completion.
  typedef base::Callback<void(const IntentExtensionList&)> ResultsCallback;

  // Requests all intent providers matching |action| and |mimetype|.
  // |mimetype| must conform to definition as outlined for
  // WebIntentsRegistry::GetIntentProviders.
  // |callback| will be invoked upon retrieving results from CWS, returning
  // a list of matching Intent extensions.
  void GetIntentProviders(const string16& action,
                          const string16& mimetype,
                          const ResultsCallback& callback);

 private:
  // Make sure that only CWSIntentsRegistryFactory can create an instance of
  // CWSIntentsRegistry.
  friend class CWSIntentsRegistryFactory;
  FRIEND_TEST_ALL_PREFIXES(CWSIntentsRegistryTest, ValidQuery);
  FRIEND_TEST_ALL_PREFIXES(CWSIntentsRegistryTest, InvalidQuery);

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // |context| is a profile-dependent URL request context. Must not be NULL.
  explicit CWSIntentsRegistry(net::URLRequestContextGetter* context);
  virtual ~CWSIntentsRegistry();

  struct IntentsQuery;

  // This is an opaque version of URLFetcher*, so we can use it as a hash key.
  typedef intptr_t URLFetcherHandle;

  // Maps URL fetchers to queries.
  typedef base::hash_map<URLFetcherHandle, IntentsQuery*> QueryMap;

  // Map for all in-flight web data requests/intent queries.
  QueryMap queries_;

  // Request context for any CWS requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(CWSIntentsRegistry);
};

#endif  // CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_H_
