// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#define CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#pragma once

#include "content/browser/geolocation/access_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// A fake (non-persisted) access token store instance useful for testing.
class FakeAccessTokenStore : public AccessTokenStore {
 public:
  FakeAccessTokenStore();

  void NotifyDelegateTokensLoaded();

  // AccessTokenStore
  MOCK_METHOD1(DoLoadAccessTokens,
               void(scoped_refptr<CancelableRequest
                    <LoadAccessTokensCallbackType> > request));
  MOCK_METHOD2(SaveAccessToken,
               void(const GURL& server_url, const string16& access_token));

  void DefaultDoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request);

  void DefaultSaveAccessToken(const GURL& server_url,
                              const string16& access_token);

  AccessTokenSet access_token_set_;
  scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request_;

 private:
  virtual ~FakeAccessTokenStore();

  DISALLOW_COPY_AND_ASSIGN(FakeAccessTokenStore);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
