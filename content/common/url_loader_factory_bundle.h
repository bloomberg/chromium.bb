// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_
#define CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

class GURL;

namespace mojo {
template <typename, typename>
struct StructTraits;
}

namespace content {
namespace mojom {
class URLLoaderFactoryBundleDataView;
}

// Holds the internal state of a URLLoaderFactoryBundle in a form that is safe
// to pass across sequences.
struct CONTENT_EXPORT URLLoaderFactoryBundleInfo {
  URLLoaderFactoryBundleInfo(URLLoaderFactoryBundleInfo&&);
  URLLoaderFactoryBundleInfo(
      network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
      std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo>
          factories_info);
  ~URLLoaderFactoryBundleInfo();

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo> factories_info;
};

// Encapsulates a collection of URLLoaderFactoryPtrs which can be usd to acquire
// loaders for various types of resource requests.
class CONTENT_EXPORT URLLoaderFactoryBundle {
 public:
  URLLoaderFactoryBundle();
  URLLoaderFactoryBundle(URLLoaderFactoryBundle&&);
  explicit URLLoaderFactoryBundle(URLLoaderFactoryBundleInfo info);
  ~URLLoaderFactoryBundle();

  URLLoaderFactoryBundle& operator=(URLLoaderFactoryBundle&&);

  // Sets the default factory to use when no registered factories match a given
  // |url|.
  void SetDefaultFactory(network::mojom::URLLoaderFactoryPtr factory);

  // Registers a new factory to handle requests matching scheme |scheme|.
  void RegisterFactory(const base::StringPiece& scheme,
                       network::mojom::URLLoaderFactoryPtr factory);

  // Returns a factory which can be used to acquire a loader for |url|. If no
  // registered factory matches |url|'s scheme, the default factory is used. It
  // is undefined behavior to call this when no default factory is set.
  network::mojom::URLLoaderFactory* GetFactoryForRequest(const GURL& url);

  // Passes out a structure which captures the internal state of this bundle in
  // a form that is safe to pass across sequences. Effectively resets |this|
  // to have no registered factories.
  URLLoaderFactoryBundleInfo PassInfo();

  // Creates a clone of this bundle which can be passed to and owned by another
  // consumer. The clone operates identically to but independent from the
  // original (this) bundle.
  URLLoaderFactoryBundle Clone();

 private:
  friend struct mojo::StructTraits<
      content::mojom::URLLoaderFactoryBundleDataView,
      URLLoaderFactoryBundle>;

  network::mojom::URLLoaderFactoryPtr default_factory_;
  std::map<std::string, network::mojom::URLLoaderFactoryPtr> factories_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryBundle);
};

}  // namespace content

#endif  // CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_
