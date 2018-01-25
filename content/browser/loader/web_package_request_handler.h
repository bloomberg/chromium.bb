// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_WEB_PACKAGE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_LOADER_WEB_PACKAGE_REQUEST_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/url_loader_request_handler.h"

namespace content {

class WebPackageLoader;

class WebPackageRequestHandler final : public URLLoaderRequestHandler {
 public:
  static bool IsSupportedMimeType(const std::string& mime_type);

  WebPackageRequestHandler();
  ~WebPackageRequestHandler() override;

  // URLLoaderRequestHandler implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback) override;
  bool MaybeCreateLoaderForResponse(
      const network::ResourceResponseHead& response,
      network::mojom::URLLoaderPtr* loader,
      network::mojom::URLLoaderClientRequest* client_request,
      ThrottlingURLLoader* url_loader) override;

 private:
  void StartResponse(network::mojom::URLLoaderRequest request,
                     network::mojom::URLLoaderClientPtr client);

  // Valid after MaybeCreateLoaderForResponse intercepts the request and until
  // the loader is re-bound to the new client for the redirected request in
  // StartResponse.
  std::unique_ptr<WebPackageLoader> web_package_loader_;

  base::WeakPtrFactory<WebPackageRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPackageRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_WEB_PACKAGE_REQUEST_HANDLER_H_
