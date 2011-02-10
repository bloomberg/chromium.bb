// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/fake_access_token_store.h"

FakeAccessTokenStore::FakeAccessTokenStore() {}

void FakeAccessTokenStore::NotifyDelegateTokensLoaded() {
  CHECK(request_ != NULL);
  request_->ForwardResult(MakeTuple(access_token_set_));
  request_ = NULL;
}

void FakeAccessTokenStore::DoLoadAccessTokens(
    scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
  DCHECK(request_ == NULL)
      << "Fake token store currently only allows one request at a time";
  request_ = request;
}

void FakeAccessTokenStore::SaveAccessToken(
    const GURL& server_url, const string16& access_token) {
  DCHECK(server_url.is_valid());
  access_token_set_[server_url] = access_token;
}

FakeAccessTokenStore::~FakeAccessTokenStore() {}
