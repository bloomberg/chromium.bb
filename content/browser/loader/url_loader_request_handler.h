// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"

namespace content {

class ResourceContext;
struct ResourceRequest;

using StartLoaderCallback =
    base::OnceCallback<void(mojom::URLLoaderRequest request,
                            mojom::URLLoaderClientPtr client)>;

using LoaderCallback = base::OnceCallback<void(StartLoaderCallback)>;

// An instance of this class is a per-request object and kept around during
// the lifetime of a request (including multiple redirect legs) on IO thread.
class CONTENT_EXPORT URLLoaderRequestHandler {
 public:
  URLLoaderRequestHandler() = default;
  virtual ~URLLoaderRequestHandler() = default;

  // Calls |callback| with a non-null StartLoaderCallback if this handler
  // can handle the request, calls it with null callback otherwise.
  virtual void MaybeCreateLoader(const ResourceRequest& resource_request,
                                 ResourceContext* resource_context,
                                 LoaderCallback callback) = 0;

  // Returns the URLLoaderFactory if any to be used for subsequent URL requests
  // going forward. Subclasses who want to handle subresource requests etc may
  // want to override this to return a custom factory.
  virtual mojom::URLLoaderFactoryPtr MaybeCreateSubresourceFactory();
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
