// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class BundledExchangesSource;
class BundledExchangesURLLoaderFactory;
class NavigationLoaderInterceptor;

// A class to provide interfaces to communicate with a BundledExchanges for
// loading. Running on the UI thread.
class BundledExchangesHandle {
 public:
  static std::unique_ptr<BundledExchangesHandle> CreateForFile();
  static std::unique_ptr<BundledExchangesHandle> CreateForTrustableFile(
      std::unique_ptr<BundledExchangesSource> source);

  ~BundledExchangesHandle();

  // Takes a NavigationLoaderInterceptor instance to handle the request for
  // a BundledExchanges, to redirect to the entry URL of the BundledExchanges,
  // and to load the main exchange from the BundledExchanges.
  std::unique_ptr<NavigationLoaderInterceptor> TakeInterceptor();

  // Creates a URLLoaderFactory to load resources from the BundledExchanges.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

  // Checks if a valid BundledExchanges is attached, opened, and ready for use.
  bool IsReadyForLoading();

 private:
  BundledExchangesHandle();

  void SetInterceptor(std::unique_ptr<NavigationLoaderInterceptor> interceptor);

  void OnBundledExchangesFileLoaded(
      std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory);

  std::unique_ptr<NavigationLoaderInterceptor> interceptor_;

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<BundledExchangesHandle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
