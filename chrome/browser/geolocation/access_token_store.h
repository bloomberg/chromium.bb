// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Geolocation access token store, and associated factory.
// An access token store is responsible for providing the API to persist a
// single network provider's access token. The factory is responsible for
// first loading tokens when required, and creating the associated token stores.
// The API is a little more complex than one might wish, due to the need for
// prefs access to happen asynchronously on the UI thread.
// These APIs are provided as abstract base classes to allow mocking and testing
// of clients, without dependency on browser process singleton objects etc.

#ifndef CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_

#include <map>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/weak_ptr.h"
#include "googleurl/src/gurl.h"

class GURL;
class PrefService;

// Provides storage for the access token used in the network request.
class AccessTokenStore : public base::RefCountedThreadSafe<AccessTokenStore> {
 public:
  static void RegisterPrefs(PrefService* prefs);

  GURL server_url() const;
  void SetAccessToken(const string16& access_token);
  bool GetAccessToken(string16* access_token) const;

 protected:
  friend class base::RefCountedThreadSafe<AccessTokenStore>;
  AccessTokenStore(
      const GURL& url, bool token_valid, const string16& initial_access_token);
  virtual ~AccessTokenStore();

  virtual void DoSetAccessToken(const string16& access_token) = 0;

 private:
  const GURL url_;
  bool access_token_valid_;
  string16 access_token_;
};

// Factory for access token stores. Asynchronously loads all access tokens, and
// calls back with a set of token stores one per server URL.
class AccessTokenStoreFactory
    : public base::RefCountedThreadSafe<AccessTokenStoreFactory> {
 public:
  typedef std::map<GURL, scoped_refptr<AccessTokenStore> > TokenStoreSet;
  class Delegate {
   public:
    virtual void OnAccessTokenStoresCreated(
        const TokenStoreSet& access_token_store) = 0;

   protected:
    virtual ~Delegate() {}
  };
  // delegate will recieve a single callback once existing access tokens have
  // been loaded from persistent store.
  // If default_url is valid, this additional URL will be added to the
  // set of access token stores returned.
  virtual void CreateAccessTokenStores(
      const base::WeakPtr<AccessTokenStoreFactory::Delegate>& delegate,
      const GURL& default_url) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AccessTokenStoreFactory>;
  virtual ~AccessTokenStoreFactory();
};

// Creates a new access token store backed by the global chome prefs.
AccessTokenStoreFactory* NewChromePrefsAccessTokenStoreFactory();

#endif  // CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
