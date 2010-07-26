// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#pragma once

#include "chrome/browser/geolocation/access_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

// A fake (non-persisted) access token store instance useful for testing.
class FakeAccessTokenStore : public AccessTokenStore {
 public:
  FakeAccessTokenStore() {}

  void NotifyDelegateTokensLoaded() {
    CHECK(request_ != NULL);
    request_->ForwardResult(MakeTuple(access_token_set_));
    request_ = NULL;
  }

  // AccessTokenStore
  virtual void DoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
    DCHECK(request_ == NULL)
        << "Fake token store currently only allows one request at a time";
    request_ = request;
  }
  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) {
    DCHECK(server_url.is_valid());
    access_token_set_[server_url] = access_token;
  }

  AccessTokenSet access_token_set_;
  scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request_;

 private:
  virtual ~FakeAccessTokenStore() {}

  DISALLOW_COPY_AND_ASSIGN(FakeAccessTokenStore);
};

#endif  // CHROME_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
