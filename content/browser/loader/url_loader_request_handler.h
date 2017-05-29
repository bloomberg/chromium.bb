// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/url_loader_factory.mojom.h"

namespace content {

class ResourceContext;
struct ResourceRequest;

using LoaderFactoryCallback =
    base::OnceCallback<void(mojom::URLLoaderFactory*)>;

// An instance of this class is a per-request object and kept around during
// the lifetime of a request (including multiple redirect legs) on IO thread.
class CONTENT_EXPORT URLLoaderRequestHandler {
 public:
  URLLoaderRequestHandler() = default;
  virtual ~URLLoaderRequestHandler() = default;

  // Calls |callback| with non-null factory if this handler can handle
  // the request, calls it with nullptr otherwise.
  // Some implementation notes:
  // 1) The returned pointer needs to be valid only until a single
  // CreateLoaderAndStart call is made, and it is okay to do CHECK(false) for
  // any subsequent calls.
  // 2) The implementor is not supposed to set up and return URLLoaderFactory
  // until it finds out that the handler is really going to handle the
  // request. (For example ServiceWorker's request handler would not need to
  // call the callback until it gets response from SW, and it may still
  // call the callback with nullptr if it turns out that it needs to fallback
  // to the network.)
  virtual void MaybeCreateLoaderFactory(const ResourceRequest& resource_request,
                                        ResourceContext* resource_context,
                                        LoaderFactoryCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
