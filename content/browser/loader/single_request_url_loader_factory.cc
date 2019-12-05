// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/single_request_url_loader_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

class SingleRequestURLLoaderFactory::HandlerState
    : public base::RefCountedThreadSafe<
          SingleRequestURLLoaderFactory::HandlerState> {
 public:
  explicit HandlerState(RequestHandler handler)
      : handler_(std::move(handler)),
        handler_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  void HandleRequest(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    if (!handler_task_runner_->RunsTasksInCurrentSequence()) {
      handler_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HandlerState::HandleRequest, this, resource_request,
                         std::move(loader), std::move(client)));
      return;
    }

    DCHECK(handler_);
    std::move(handler_).Run(resource_request, std::move(loader),
                            std::move(client));
  }

 private:
  friend class base::RefCountedThreadSafe<HandlerState>;

  ~HandlerState() {
    if (handler_) {
      handler_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DropHandlerOnHandlerSequence, std::move(handler_)));
    }
  }

  static void DropHandlerOnHandlerSequence(RequestHandler handler) {}

  RequestHandler handler_;
  const scoped_refptr<base::SequencedTaskRunner> handler_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(HandlerState);
};

class SingleRequestURLLoaderFactory::PendingFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit PendingFactory(scoped_refptr<HandlerState> state)
      : state_(std::move(state)) {}
  ~PendingFactory() override = default;

  // PendingSharedURLLoaderFactory:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return new SingleRequestURLLoaderFactory(std::move(state_));
  }

 private:
  scoped_refptr<HandlerState> state_;

  DISALLOW_COPY_AND_ASSIGN(PendingFactory);
};

SingleRequestURLLoaderFactory::SingleRequestURLLoaderFactory(
    RequestHandler handler)
    : state_(base::MakeRefCounted<HandlerState>(std::move(handler))) {}

void SingleRequestURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  state_->HandleRequest(request, std::move(loader), std::move(client));
}

void SingleRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  // Pass |this| as the recevier context to make sure this object stays alive
  // while it still has receivers.
  receivers_.Add(this, std::move(receiver), this);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
SingleRequestURLLoaderFactory::Clone() {
  return std::make_unique<PendingFactory>(state_);
}

SingleRequestURLLoaderFactory::SingleRequestURLLoaderFactory(
    scoped_refptr<HandlerState> state)
    : state_(std::move(state)) {}

SingleRequestURLLoaderFactory::~SingleRequestURLLoaderFactory() = default;

}  // namespace content
