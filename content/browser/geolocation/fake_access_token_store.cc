// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/fake_access_token_store.h"

using testing::_;
using testing::Invoke;

FakeAccessTokenStore::FakeAccessTokenStore() {
  ON_CALL(*this, DoLoadAccessTokens(_))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultDoLoadAccessTokens));
  ON_CALL(*this, SaveAccessToken(_, _))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultSaveAccessToken));
}

void FakeAccessTokenStore::NotifyDelegateTokensLoaded() {
  CHECK(request_ != NULL);
  net::URLRequestContextGetter* context_getter = NULL;
  request_->ForwardResult(MakeTuple(access_token_set_, context_getter));
  request_ = NULL;
}

void FakeAccessTokenStore::DefaultDoLoadAccessTokens(
    scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
  DCHECK(request_ == NULL)
      << "Fake token store currently only allows one request at a time";
  request_ = request;
}

void FakeAccessTokenStore::DefaultSaveAccessToken(
    const GURL& server_url, const string16& access_token) {
  DCHECK(server_url.is_valid());
  access_token_set_[server_url] = access_token;
}

FakeAccessTokenStore::~FakeAccessTokenStore() {}
