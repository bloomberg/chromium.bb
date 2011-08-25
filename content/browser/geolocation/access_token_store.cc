// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/access_token_store.h"

AccessTokenStore::AccessTokenStore() {
}

AccessTokenStore::~AccessTokenStore() {
}

AccessTokenStore::Handle AccessTokenStore::LoadAccessTokens(
    CancelableRequestConsumerBase* consumer,
    LoadAccessTokensCallbackType* callback) {
  scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request(
      new CancelableRequest<LoadAccessTokensCallbackType>(callback));
  AddRequest(request, consumer);
  DCHECK(request->handle());

  DoLoadAccessTokens(request);
  return request->handle();
}
