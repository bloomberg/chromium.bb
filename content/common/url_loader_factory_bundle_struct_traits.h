// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_
#define CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_

#include "content/common/url_loader_factory_bundle.h"
#include "content/common/url_loader_factory_bundle.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::URLLoaderFactoryBundleDataView,
                    content::URLLoaderFactoryBundle> {
  static network::mojom::URLLoaderFactoryPtr default_factory(
      content::URLLoaderFactoryBundle& bundle);

  static std::map<std::string, network::mojom::URLLoaderFactoryPtr> factories(
      content::URLLoaderFactoryBundle& bundle);

  static bool Read(content::mojom::URLLoaderFactoryBundleDataView data,
                   content::URLLoaderFactoryBundle* out_bundle);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_
