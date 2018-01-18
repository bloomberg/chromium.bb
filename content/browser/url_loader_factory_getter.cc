// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/storage_partition_impl.h"
#include "services/network/public/interfaces/network_service.mojom.h"

namespace content {

namespace {
base::LazyInstance<URLLoaderFactoryGetter::GetNetworkFactoryCallback>::Leaky
    g_get_network_factory_callback = LAZY_INSTANCE_INITIALIZER;
}

URLLoaderFactoryGetter::URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::Initialize(StoragePartitionImpl* partition) {
  DCHECK(partition);
  partition_ = partition;

  network::mojom::URLLoaderFactoryPtr network_factory;
  HandleNetworkFactoryRequestOnUIThread(MakeRequest(&network_factory));

  network::mojom::URLLoaderFactoryPtr blob_factory;
  partition_->GetBlobURLLoaderFactory()->HandleRequest(
      mojo::MakeRequest(&blob_factory));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderFactoryGetter::InitializeOnIOThread, this,
                     network_factory.PassInterface(),
                     blob_factory.PassInterface()));
}

void URLLoaderFactoryGetter::OnStoragePartitionDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  partition_ = nullptr;
}

network::mojom::URLLoaderFactory* URLLoaderFactoryGetter::GetNetworkFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (g_get_network_factory_callback.Get() && !test_factory_)
    g_get_network_factory_callback.Get().Run(this);

  if (test_factory_)
    return test_factory_;

  if (!network_factory_.is_bound() || network_factory_.encountered_error()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread,
            this, mojo::MakeRequest(&network_factory_)));
  }
  return network_factory_.get();
}

network::mojom::URLLoaderFactory* URLLoaderFactoryGetter::GetBlobFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return blob_factory_.get();
}

void URLLoaderFactoryGetter::SetNetworkFactoryForTesting(
    network::mojom::URLLoaderFactory* test_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!test_factory_ || !test_factory);
  test_factory_ = test_factory;
}

void URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
    const GetNetworkFactoryCallback& get_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!g_get_network_factory_callback.Get() ||
         !get_network_factory_callback);
  g_get_network_factory_callback.Get() = get_network_factory_callback;
}

void URLLoaderFactoryGetter::FlushNetworkInterfaceOnIOThreadForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderFactoryGetter::FlushNetworkInterfaceForTesting,
                     this),
      run_loop.QuitClosure());
  run_loop.Run();
}

void URLLoaderFactoryGetter::FlushNetworkInterfaceForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (network_factory_)
    network_factory_.FlushForTesting();
}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::InitializeOnIOThread(
    network::mojom::URLLoaderFactoryPtrInfo network_factory,
    network::mojom::URLLoaderFactoryPtrInfo blob_factory) {
  network_factory_.Bind(std::move(network_factory));
  blob_factory_.Bind(std::move(blob_factory));
}

void URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread(
    network::mojom::URLLoaderFactoryRequest network_factory_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |StoragePartitionImpl| may have went away while |URLLoaderFactoryGetter| is
  // still held by consumers.
  if (!partition_)
    return;
  partition_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(network_factory_request), 0);
}

}  // namespace content
