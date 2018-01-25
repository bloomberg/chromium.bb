// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_request_handler.h"

#include "content/common/navigation_subresource_loader_params.h"

namespace content {

base::Optional<SubresourceLoaderParams>
URLLoaderRequestHandler::MaybeCreateSubresourceLoaderParams() {
  return base::nullopt;
}

bool URLLoaderRequestHandler::MaybeCreateLoaderForResponse(
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* loader,
    network::mojom::URLLoaderClientRequest* client_request,
    ThrottlingURLLoader* url_loader) {
  return false;
}

}  // namespace content
