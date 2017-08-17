// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_url_loader_factory_getter.h"

namespace content {

ChildURLLoaderFactoryGetter::ChildURLLoaderFactoryGetter(
    PossiblyAssociatedURLLoaderFactory network_loader_factory,
    URLLoaderFactoryGetterCallback blob_loader_factory_getter)
    : network_loader_factory_(std::move(network_loader_factory)),
      blob_loader_factory_getter_(std::move(blob_loader_factory_getter)) {}

mojom::URLLoaderFactory*
ChildURLLoaderFactoryGetter::GetNetworkLoaderFactory() {
  return network_loader_factory_.get();
}

mojom::URLLoaderFactory* ChildURLLoaderFactoryGetter::GetBlobLoaderFactory() {
  if (!blob_loader_factory_) {
    DCHECK(!blob_loader_factory_getter_.is_null());
    blob_loader_factory_ = std::move(blob_loader_factory_getter_).Run();
  }
  return blob_loader_factory_.get();
}

ChildURLLoaderFactoryGetter::~ChildURLLoaderFactoryGetter() = default;

}  // namespace content
