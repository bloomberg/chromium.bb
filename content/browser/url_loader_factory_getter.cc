// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include "base/bind.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/network_service.mojom.h"

namespace content {

URLLoaderFactoryGetter::URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::Initialize(StoragePartitionImpl* partition) {
  mojom::URLLoaderFactoryPtr network_factory;
  partition->network_context()->CreateURLLoaderFactory(
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

mojom::URLLoaderFactoryPtr* URLLoaderFactoryGetter::GetNetworkFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return test_factory_.is_bound() ? &test_factory_ : &network_factory_;
}

mojom::URLLoaderFactoryPtr* URLLoaderFactoryGetter::GetBlobFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return &blob_factory_;
}

void URLLoaderFactoryGetter::SetNetworkFactoryForTesting(
    mojom::URLLoaderFactoryPtr test_factory) {
  // Since the URLLoaderFactory pointers are bound on the IO thread, and this
  // method is called on the UI thread, we are not able to unbind and return the
  // old value. As such this class keeps two separate pointers, one for test.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderFactoryGetter::SetTestNetworkFactoryOnIOThread,
                     this, test_factory.PassInterface()));
}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::InitializeOnIOThread(
    mojom::URLLoaderFactoryPtrInfo network_factory,
    mojom::URLLoaderFactoryPtrInfo blob_factory) {
  network_factory_.Bind(std::move(network_factory));
  blob_factory_.Bind(std::move(blob_factory));
}

void URLLoaderFactoryGetter::SetTestNetworkFactoryOnIOThread(
    mojom::URLLoaderFactoryPtrInfo test_factory) {
  test_factory_.Bind(std::move(test_factory));
}

}  // namespace content
