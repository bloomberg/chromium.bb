// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#define CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#pragma once

#include "content/browser/geolocation/access_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

// A fake (non-persisted) access token store instance useful for testing.
class FakeAccessTokenStore : public AccessTokenStore {
 public:
  FakeAccessTokenStore();

  void NotifyDelegateTokensLoaded();

  // AccessTokenStore
  virtual void DoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request);
  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token);

  AccessTokenSet access_token_set_;
  scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request_;

 private:
  virtual ~FakeAccessTokenStore();

  DISALLOW_COPY_AND_ASSIGN(FakeAccessTokenStore);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
