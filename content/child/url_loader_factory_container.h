// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_URL_LOADER_FACTORY_CONTAINER_H_
#define CONTENT_CHILD_URL_LOADER_FACTORY_CONTAINER_H_

#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

// Contains default URLLoaderFactory's.
class URLLoaderFactoryContainer {
 public:
  using PossiblyAssociatedURLLoaderFactory =
      PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>;

  URLLoaderFactoryContainer(
      PossiblyAssociatedURLLoaderFactory network_loader_factory,
      mojom::URLLoaderFactoryPtr blob_loader_factory);
  ~URLLoaderFactoryContainer();

  mojom::URLLoaderFactory* network_loader_factory() const {
    return network_loader_factory_.get();
  }

  mojom::URLLoaderFactory* blob_loader_factory() const {
    return blob_loader_factory_.get();
  }

 private:
  PossiblyAssociatedURLLoaderFactory network_loader_factory_;
  mojom::URLLoaderFactoryPtr blob_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_CHILD_URL_LOADER_FACTORY_CONTAINER_H_
