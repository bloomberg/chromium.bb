// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_URL_LOADER_FACTORY_IMPL_H_
#define MOJO_SERVICES_NETWORK_URL_LOADER_FACTORY_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"

namespace mojo {
class NetworkContext;

class URLLoaderFactoryImpl : public URLLoaderFactory {
 public:
  URLLoaderFactoryImpl(NetworkContext* context,
                       scoped_ptr<mojo::AppRefCount> app_refcount,
                       InterfaceRequest<URLLoaderFactory> request);
  ~URLLoaderFactoryImpl() override;

  // URLLoaderFactory methods:
  void CreateURLLoader(InterfaceRequest<URLLoader> loader) override;

 private:
  NetworkContext* context_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;
  StrongBinding<URLLoaderFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_URL_LOADER_FACTORY_IMPL_H_
