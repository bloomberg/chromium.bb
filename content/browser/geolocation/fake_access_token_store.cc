// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/fake_access_token_store.h"

#include "base/logging.h"

using testing::_;
using testing::Invoke;

FakeAccessTokenStore::FakeAccessTokenStore() {
  ON_CALL(*this, LoadAccessTokens(_))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultLoadAccessTokens));
  ON_CALL(*this, SaveAccessToken(_, _))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultSaveAccessToken));
}

void FakeAccessTokenStore::NotifyDelegateTokensLoaded() {
  net::URLRequestContextGetter* context_getter = NULL;
  callback_.Run(access_token_set_, context_getter);
}

void FakeAccessTokenStore::DefaultLoadAccessTokens(
    const LoadAccessTokensCallbackType& callback) {
  callback_ = callback;
}

void FakeAccessTokenStore::DefaultSaveAccessToken(
    const GURL& server_url, const string16& access_token) {
  DCHECK(server_url.is_valid());
  access_token_set_[server_url] = access_token;
}

FakeAccessTokenStore::~FakeAccessTokenStore() {}
