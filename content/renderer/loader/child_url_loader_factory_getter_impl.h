// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_GETTER_IMPL_H_
#define CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_GETTER_IMPL_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

class CONTENT_EXPORT ChildURLLoaderFactoryGetterImpl
    : public ChildURLLoaderFactoryGetter {
 public:
  using PossiblyAssociatedURLLoaderFactory =
      PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>;
  using URLLoaderFactoryGetterCallback =
      base::OnceCallback<network::mojom::URLLoaderFactoryPtr()>;

  ChildURLLoaderFactoryGetterImpl();

  ChildURLLoaderFactoryGetterImpl(
      PossiblyAssociatedURLLoaderFactory network_loader_factory,
      URLLoaderFactoryGetterCallback blob_loader_factory_getter);

  ChildURLLoaderFactoryGetterImpl(
      PossiblyAssociatedURLLoaderFactory network_loader_factory,
      PossiblyAssociatedURLLoaderFactory blob_loader_factory_getter);

  Info GetClonedInfo() override;

  network::mojom::URLLoaderFactory* GetFactoryForURL(
      const GURL& url,
      network::mojom::URLLoaderFactory* default_factory) override;
  network::mojom::URLLoaderFactory* GetNetworkLoaderFactory() override;
  network::mojom::URLLoaderFactory* GetBlobLoaderFactory() override;

 private:
  ~ChildURLLoaderFactoryGetterImpl() override;

  PossiblyAssociatedURLLoaderFactory network_loader_factory_;

  // Either factory_getter or factory is non-null (to support
  // lazy instantiation), or both could be null (if the default
  // ctor is used).
  URLLoaderFactoryGetterCallback blob_loader_factory_getter_;
  PossiblyAssociatedURLLoaderFactory blob_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_GETTER_IMPL_H_
