// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/url_loader_impl.h"

namespace mojo {

NetworkServiceImpl::NetworkServiceImpl(ApplicationConnection* connection,
                                       NetworkContext* context)
    : context_(context),
      origin_(GURL(connection->GetRemoteApplicationURL()).GetOrigin()) {
}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::CreateURLLoader(InterfaceRequest<URLLoader> loader) {
  // TODO(darin): Plumb origin_. Use for CORS.
  BindToRequest(new URLLoaderImpl(context_), &loader);
}

void NetworkServiceImpl::GetCookieStore(InterfaceRequest<CookieStore> store) {
  BindToRequest(new CookieStoreImpl(context_, origin_), &store);
}

}  // namespace mojo
