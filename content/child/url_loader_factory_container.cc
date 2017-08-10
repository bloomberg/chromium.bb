// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/url_loader_factory_container.h"

namespace content {

URLLoaderFactoryContainer::URLLoaderFactoryContainer(
    PossiblyAssociatedURLLoaderFactory network_loader_factory,
    mojom::URLLoaderFactoryPtr blob_loader_factory)
    : network_loader_factory_(std::move(network_loader_factory)),
      blob_loader_factory_(std::move(blob_loader_factory)) {}

URLLoaderFactoryContainer::~URLLoaderFactoryContainer() = default;

}  // namespace content
