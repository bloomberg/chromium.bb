// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Geolocation access token store, and associated factory function.
// An access token store is responsible for providing the API to persist
// access tokens, one at a time, and to load them back on mass.
// The API is a little more complex than one might wish, due to the need for
// prefs access to happen asynchronously on the UI thread.
// This API is provided as abstract base classes to allow mocking and testing
// of clients, without dependency on browser process singleton objects etc.

#ifndef CONTENT_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
#define CONTENT_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
#pragma once

#include <map>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "content/browser/cancelable_request.h"
#include "googleurl/src/gurl.h"

class GURL;
class PrefService;

// Provides storage for the access token used in the network request.
class AccessTokenStore : public base::RefCountedThreadSafe<AccessTokenStore>,
                         public CancelableRequestProvider {
 public:
  static void RegisterPrefs(PrefService* prefs);

  // Map of server URLs to associated access token.
  typedef std::map<GURL, string16> AccessTokenSet;
  typedef Callback1<AccessTokenSet>::Type LoadAccessTokensCallbackType;
  // callback will be invoked once, after existing access tokens have
  // been loaded from persistent store. Takes ownership of callback.
  // Returns a handle which can subsequently be used with CancelRequest().
  Handle LoadAccessTokens(CancelableRequestConsumerBase* consumer,
                          LoadAccessTokensCallbackType* callback);

  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AccessTokenStore>;
  AccessTokenStore();
  virtual ~AccessTokenStore();

  virtual void DoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > req) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessTokenStore);
};

// Creates a new access token store backed by the global chome prefs.
AccessTokenStore* NewChromePrefsAccessTokenStore();

#endif  // CONTENT_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
