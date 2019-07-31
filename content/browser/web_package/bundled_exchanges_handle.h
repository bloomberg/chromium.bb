// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class BundledExchangesReader;
class NavigationLoaderInterceptor;

// A class to provide interfaces to communicate with a BundledExchanges for
// loading. Running on the UI thread.
// TODO(crbug.com/966753): Add browser tests once core parts are submitted and
// basic sequence can pass.
class BundledExchangesHandle final {
 public:
  explicit BundledExchangesHandle(const BundledExchangesSource& source);
  ~BundledExchangesHandle();

  // Creates a NavigationLoaderInterceptor instance to handle the request for
  // a BundledExchanges, to redirect to the entry URL of the BundledExchanges,
  // and to load the main exchange from the BundledExchanges.
  std::unique_ptr<NavigationLoaderInterceptor> CreateInterceptor();

  // Creates a URLLoaderFactory to load resources from the BundledExchanges.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

 private:
  class StartURLRedirectLoader;

  void CreateStartURLLoader(const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderRequest request,
                            network::mojom::URLLoaderClientPtr client);
  void MayRedirectStartURLLoader();
  void OnMetadataReady(base::Optional<std::string> error);

  const BundledExchangesSource source_;

  base::WeakPtr<StartURLRedirectLoader> redirect_loader_;
  std::unique_ptr<BundledExchangesReader> reader_;
  GURL start_url_;
  base::Optional<std::string> start_url_error_;
  bool is_redirected_ = false;

  base::WeakPtrFactory<BundledExchangesHandle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
