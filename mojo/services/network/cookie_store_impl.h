// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_
#define MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_

#include "mojo/services/public/interfaces/network/cookie_store.mojom.h"
#include "url/gurl.h"

namespace mojo {
class NetworkContext;

class CookieStoreImpl : public InterfaceImpl<CookieStore> {
 public:
  CookieStoreImpl(NetworkContext* context, const GURL& origin);
  virtual ~CookieStoreImpl();

 private:
  // CookieStore methods:
  virtual void Get(const String& url,
                   const Callback<void(String)>& callback) OVERRIDE;
  virtual void Set(const String& url, const String& cookie,
                   const Callback<void(bool)>& callback) OVERRIDE;

  NetworkContext* context_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_COOKIE_STORE_IMPL_H_
