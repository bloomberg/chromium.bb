// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/network_context_manager.h"

#include <string>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"

namespace chromecast {

NetworkContextManager::NetworkContextManager(
    net::URLRequestContextGetter* url_request_context_getter)
    : NetworkContextManager(url_request_context_getter, nullptr) {}

NetworkContextManager::NetworkContextManager(
    net::URLRequestContextGetter* url_request_context_getter,
    std::unique_ptr<network::NetworkService> network_service)
    : network_service_for_test_(std::move(network_service)),
      url_request_context_getter_(url_request_context_getter),
      weak_factory_(this) {
  DCHECK(url_request_context_getter_);
  // The NetworkContext must be initialized on the browser's IO thread. Posting
  // this task from the constructor ensures that |network_context_| will
  // be initialized for subsequent calls to BindRequestOnIOThread().
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&NetworkContextManager::InitializeOnIoThread,
                     weak_factory_.GetWeakPtr()));
}

NetworkContextManager::~NetworkContextManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void NetworkContextManager::InitializeOnIoThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  network::NetworkService* network_service =
      network_service_for_test_ ? network_service_for_test_.get()
                                : content::GetNetworkServiceImpl();
  network_context_ = std::make_unique<network::NetworkContext>(
      network_service, mojo::MakeRequest(&network_context_ptr_),
      url_request_context_getter_);
}

void NetworkContextManager::BindRequestOnIOThread(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  network_context_->CreateURLLoaderFactory(std::move(request),
                                           0 /* process_id */);
}

network::mojom::URLLoaderFactoryPtr
NetworkContextManager::GetURLLoaderFactory() {
  network::mojom::URLLoaderFactoryPtr loader_factory;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&NetworkContextManager::BindRequestOnIOThread,
                     weak_factory_.GetWeakPtr(),
                     mojo::MakeRequest(&loader_factory)));
  return loader_factory;
}

//  static
NetworkContextManager* NetworkContextManager::CreateForTest(
    net::URLRequestContextGetter* url_request_context_getter,
    std::unique_ptr<network::NetworkService> network_service) {
  return new NetworkContextManager(url_request_context_getter,
                                   std::move(network_service));
}

}  // namespace chromecast