// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "net/url_request/redirect_info.h"

namespace content {

class ResourceContext;
struct ResourceRequest;
struct SubresourceLoaderParams;

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

  // Returns a SubresourceLoaderParams if any to be used for subsequent URL
  // requests going forward. Subclasses who want to set-up custom loader for
  // subresource requests may want to override this.
  // Note that the handler can return a null callback to MaybeCreateLoader(),
  // and at the same time can return non-null SubresourceLoaderParams here if it
  // does NOT want to handle the specific request given to MaybeCreateLoader()
  // but wants to handle the subsequent resource requests.
  virtual base::Optional<SubresourceLoaderParams>
  MaybeCreateSubresourceLoaderParams();

  // Returns true if the handler creates a loader for the |response| passed.
  // An example of where this is used is AppCache, where the handler returns
  // fallback content for the response passed in.
  // The URLLoader interface pointer is returned in the |loader| parameter.
  // The interface request for the URLLoaderClient is returned in the
  // |client_request| parameter.
  virtual bool MaybeCreateLoaderForResponse(
      const ResourceResponseHead& response,
      mojom::URLLoaderPtr* loader,
      mojom::URLLoaderClientRequest* client_request);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_REQUEST_HANDLER_H_
