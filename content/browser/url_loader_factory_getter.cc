// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/common/network_service.mojom.h"

namespace content {

namespace {
base::LazyInstance<URLLoaderFactoryGetter::GetNetworkFactoryCallback>::Leaky
    g_get_network_factory_callback = LAZY_INSTANCE_INITIALIZER;
}

URLLoaderFactoryGetter::URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::Initialize(StoragePartitionImpl* partition) {
  mojom::URLLoaderFactoryPtr network_factory;
  partition->GetNetworkContext()->CreateURLLoaderFactory(
      MakeRequest(&network_factory), 0);

  mojom::URLLoaderFactoryPtr blob_factory;
  partition->GetBlobURLLoaderFactory()->HandleRequest(
      mojo::MakeRequest(&blob_factory));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderFactoryGetter::InitializeOnIOThread, this,
                     network_factory.PassInterface(),
                     blob_factory.PassInterface()));
}

mojom::URLLoaderFactory* URLLoaderFactoryGetter::GetNetworkFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (g_get_network_factory_callback.Get() && !test_factory_)
    g_get_network_factory_callback.Get().Run(this);
  return test_factory_ ? test_factory_ : network_factory_.get();
}

mojom::URLLoaderFactory* URLLoaderFactoryGetter::GetBlobFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return blob_factory_.get();
}

void URLLoaderFactoryGetter::SetNetworkFactoryForTesting(
    mojom::URLLoaderFactory* test_factory) {
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

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::InitializeOnIOThread(
    mojom::URLLoaderFactoryPtrInfo network_factory,
    mojom::URLLoaderFactoryPtrInfo blob_factory) {
  network_factory_.Bind(std::move(network_factory));
  blob_factory_.Bind(std::move(blob_factory));
}

}  // namespace content
