// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/safe_browsing_network_context.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace safe_browsing {

class SafeBrowsingNetworkContext::SharedURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  explicit SharedURLLoaderFactory(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter)
      : request_context_getter_(request_context_getter) {}

  void Reset() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_loader_factory_.reset();
    network_context_.reset();
    request_context_getter_ = nullptr;
    if (internal_state_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&InternalState::Reset, internal_state_));
    }
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!network_context_) {
      internal_state_ = base::MakeRefCounted<InternalState>();
      internal_state_->Initialize(request_context_getter_,
                                  MakeRequest(&network_context_));
    }
    return network_context_.get();
  }

 protected:
  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED();
    return nullptr;
  }

  // network::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!url_loader_factory_) {
      GetNetworkContext()->CreateURLLoaderFactory(
          MakeRequest(&url_loader_factory_), 0);
    }
    return url_loader_factory_.get();
  }

 private:
  // This class holds on to the network::NetworkContext object on the IO thread.
  class InternalState : public base::RefCountedThreadSafe<InternalState> {
   public:
    InternalState() = default;

    void Initialize(
        scoped_refptr<net::URLRequestContextGetter> request_context_getter,
        network::mojom::NetworkContextRequest network_context_request) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&InternalState::InitOnIO, this, request_context_getter,
                         std::move(network_context_request)));
    }

    void Reset() {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
      network_context_impl_.reset();
    }

   private:
    friend class base::RefCountedThreadSafe<InternalState>;
    virtual ~InternalState() {}

    void InitOnIO(
        scoped_refptr<net::URLRequestContextGetter> request_context_getter,
        network::mojom::NetworkContextRequest network_context_request) {
      network_context_impl_ = std::make_unique<network::NetworkContext>(
          nullptr, std::move(network_context_request), request_context_getter);
    }

    std::unique_ptr<network::NetworkContext> network_context_impl_;

    DISALLOW_COPY_AND_ASSIGN(InternalState);
  };

  friend class base::RefCounted<SharedURLLoaderFactory>;
  ~SharedURLLoaderFactory() override = default;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  network::mojom::NetworkContextPtr network_context_;
  network::mojom::URLLoaderFactoryPtr url_loader_factory_;
  scoped_refptr<InternalState> internal_state_;

  DISALLOW_COPY_AND_ASSIGN(SharedURLLoaderFactory);
};

SafeBrowsingNetworkContext::SafeBrowsingNetworkContext(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  url_loader_factory_ =
      base::MakeRefCounted<SharedURLLoaderFactory>(request_context_getter);
}

SafeBrowsingNetworkContext::~SafeBrowsingNetworkContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingNetworkContext::GetURLLoaderFactory() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return url_loader_factory_;
}

network::mojom::NetworkContext*
SafeBrowsingNetworkContext::GetNetworkContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return url_loader_factory_->GetNetworkContext();
}

void SafeBrowsingNetworkContext::ServiceShuttingDown() {
  url_loader_factory_->Reset();
}

}  // namespace safe_browsing
