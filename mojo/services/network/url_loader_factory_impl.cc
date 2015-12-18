// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/url_loader_factory_impl.h"

#include <utility>

#include "mojo/services/network/url_loader_impl.h"

namespace mojo {

URLLoaderFactoryImpl::URLLoaderFactoryImpl(
    NetworkContext* context,
    scoped_ptr<mojo::AppRefCount> app_refcount,
    InterfaceRequest<URLLoaderFactory> request)
    : context_(context),
      app_refcount_(std::move(app_refcount)),
      binding_(this, std::move(request)) {}

URLLoaderFactoryImpl::~URLLoaderFactoryImpl() {
}

void URLLoaderFactoryImpl::CreateURLLoader(InterfaceRequest<URLLoader> loader) {
  // TODO(darin): Plumb origin. Use for CORS.
  // TODO(beng): Figure out how to get origin through to here.
  // The loader will delete itself when the pipe is closed, unless a request is
  // in progress. In which case, the loader will delete itself when the request
  // is finished.
  new URLLoaderImpl(context_, std::move(loader), app_refcount_->Clone());
}

}  // namespace mojo
