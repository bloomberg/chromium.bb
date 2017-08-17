// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_CHILD_CHILD_URL_LOADER_FACTORY_GETTER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

// ChildProcess version's URLLoaderFactoryGetter, i.e. a getter that holds
// on to URLLoaderFactory's for a given loading context (e.g. a frame)
// and allows code to access them.
class ChildURLLoaderFactoryGetter
    : public base::RefCounted<ChildURLLoaderFactoryGetter> {
 public:
  using PossiblyAssociatedURLLoaderFactory =
      PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>;
  using URLLoaderFactoryGetterCallback =
      base::OnceCallback<mojom::URLLoaderFactoryPtr()>;

  ChildURLLoaderFactoryGetter(
      PossiblyAssociatedURLLoaderFactory network_loader_factory,
      URLLoaderFactoryGetterCallback blob_loader_factory_getter);

  mojom::URLLoaderFactory* GetNetworkLoaderFactory();
  mojom::URLLoaderFactory* GetBlobLoaderFactory();

 private:
  friend class base::RefCounted<ChildURLLoaderFactoryGetter>;
  ~ChildURLLoaderFactoryGetter();

  PossiblyAssociatedURLLoaderFactory network_loader_factory_;

  // Either factory_getter or factory is non-null (to support
  // lazy instantiation).
  URLLoaderFactoryGetterCallback blob_loader_factory_getter_;
  PossiblyAssociatedURLLoaderFactory blob_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_URL_LOADER_FACTORY_GETTER_H_
