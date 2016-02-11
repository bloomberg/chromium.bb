// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_
#define MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/cookie_store.mojom.h"
#include "mojo/shell/public/cpp/shell.h"
#include "url/gurl.h"

namespace mojo {
class NetworkContext;

class CookieStoreImpl : public CookieStore {
 public:
  CookieStoreImpl(NetworkContext* context,
                  const GURL& origin,
                  scoped_ptr<mojo::AppRefCount> app_refcount,
                  InterfaceRequest<CookieStore> request);
  ~CookieStoreImpl() override;

 private:
  // CookieStore methods:
  void Get(const String& url, const Callback<void(String)>& callback) override;
  void Set(const String& url,
           const String& cookie,
           const Callback<void(bool)>& callback) override;

  NetworkContext* context_;
  GURL origin_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;
  StrongBinding<CookieStore> binding_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_
