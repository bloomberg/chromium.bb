// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/cookie_store_impl.h"

#include <utility>

#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/network_context.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"

namespace mojo {
namespace {

void AdaptGetCookiesCallback(const Callback<void(String)>& callback,
                             const std::string& cookies) {
  callback.Run(cookies);
}

void AdaptSetCookieCallback(const Callback<void(bool)>& callback,
                            bool success) {
  callback.Run(success);
}

}  // namespace

CookieStoreImpl::CookieStoreImpl(NetworkContext* context,
                                 const GURL& origin,
                                 scoped_ptr<mojo::AppRefCount> app_refcount,
                                 InterfaceRequest<CookieStore> request)
    : context_(context),
      origin_(origin),
      app_refcount_(std::move(app_refcount)),
      binding_(this, std::move(request)) {}

CookieStoreImpl::~CookieStoreImpl() {
}

void CookieStoreImpl::Get(const String& url,
                          const Callback<void(String)>& callback) {
  // TODO(darin): Perform origin restriction.
  net::CookieStore* store = context_->url_request_context()->cookie_store();
  if (!store) {
    callback.Run(String());
    return;
  }
  store->GetCookiesWithOptionsAsync(
      url.To<GURL>(),
      net::CookieOptions(),
      base::Bind(&AdaptGetCookiesCallback, callback));
}

void CookieStoreImpl::Set(const String& url,
                          const String& cookie,
                          const Callback<void(bool)>& callback) {
  // TODO(darin): Perform origin restriction.
  net::CookieStore* store = context_->url_request_context()->cookie_store();
  if (!store) {
    callback.Run(false);
    return;
  }
  store->SetCookieWithOptionsAsync(
      url.To<GURL>(),
      cookie,
      net::CookieOptions(),
      base::Bind(&AdaptSetCookieCallback, callback));
}

}  // namespace mojo
