// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_

#include "chrome/browser/geolocation/access_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

// A fake (non-persisted) access token store instance useful for testing.
class FakeAccessTokenStore : public AccessTokenStore {
 public:
  explicit FakeAccessTokenStore(const GURL& url)
      : AccessTokenStore(url, false, string16()) {}

  virtual void DoSetAccessToken(const string16& access_token) {
    access_token_set_ = access_token;
  }

  string16 access_token_set_;

 private:
  virtual ~FakeAccessTokenStore() {}
};

class FakeAccessTokenStoreFactory : public AccessTokenStoreFactory {
 public:
  void NotifyDelegateStoreCreated() {
    ASSERT_TRUE(delegate_ != NULL);
    AccessTokenStoreFactory::TokenStoreSet token_stores;
    token_stores[default_url_] = new FakeAccessTokenStore(default_url_);
    delegate_->OnAccessTokenStoresCreated(token_stores);
  }

  // AccessTokenStoreFactory
  virtual void CreateAccessTokenStores(
      const base::WeakPtr<AccessTokenStoreFactory::Delegate>& delegate,
      const GURL& default_url) {
    delegate_ = delegate;
    default_url_ = default_url;
  }

  base::WeakPtr<AccessTokenStoreFactory::Delegate> delegate_;
  GURL default_url_;
};

#endif  // CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
