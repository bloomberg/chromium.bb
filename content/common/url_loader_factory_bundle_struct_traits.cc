// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_loader_factory_bundle_struct_traits.h"

namespace mojo {

using Traits = StructTraits<content::mojom::URLLoaderFactoryBundleDataView,
                            content::URLLoaderFactoryBundle>;

// static
network::mojom::URLLoaderFactoryPtr Traits::default_factory(
    content::URLLoaderFactoryBundle& bundle) {
  return std::move(bundle.default_factory_);
}

// static
std::map<std::string, network::mojom::URLLoaderFactoryPtr> Traits::factories(
    content::URLLoaderFactoryBundle& bundle) {
  return std::move(bundle.factories_);
}

// static
bool Traits::Read(content::mojom::URLLoaderFactoryBundleDataView data,
                  content::URLLoaderFactoryBundle* out_bundle) {
  out_bundle->SetDefaultFactory(
      data.TakeDefaultFactory<network::mojom::URLLoaderFactoryPtr>());
  if (!data.ReadFactories(&out_bundle->factories_))
    return false;
  return true;
}

}  // namespace mojo
