// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_factory.h"

#include "base/bind.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/public/browser/browser_thread.h"

namespace {

// A class to inherit NavigationLoaderInterceptor for the navigation to a
// BundledExchanges.
// TODO(crbug.com/966753): Implement.
class Interceptor final : public content::NavigationLoaderInterceptor {
 public:
  Interceptor() { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }
  ~Interceptor() override { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         content::BrowserContext* browser_context,
                         content::ResourceContext* resource_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::move(callback).Run({});
  }

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

}  // namespace

namespace content {

BundledExchangesFactory::BundledExchangesFactory(
    const BundledExchangesSource& bundled_exchanges_source)
    : reader_(
          std::make_unique<BundledExchangesReader>(bundled_exchanges_source)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  reader_->ReadMetadata(base::BindOnce(
      &BundledExchangesFactory::OnMetadataReady, weak_factory_.GetWeakPtr()));
}

BundledExchangesFactory::~BundledExchangesFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::unique_ptr<NavigationLoaderInterceptor>
BundledExchangesFactory::CreateInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<Interceptor>();
}

void BundledExchangesFactory::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NOTREACHED();
}

void BundledExchangesFactory::OnMetadataReady(
    base::Optional<std::string> error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

}  // namespace content
