// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CHILD_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_PUBLIC_RENDERER_CHILD_URL_LOADER_FACTORY_GETTER_H_

#include "base/memory/ref_counted.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

class GURL;

namespace content {

// ChildProcess version's URLLoaderFactoryGetter, i.e. a getter that holds
// on to URLLoaderFactory's for a given loading context (e.g. a frame)
// and allows code to access them.
class ChildURLLoaderFactoryGetter
    : public base::RefCounted<ChildURLLoaderFactoryGetter> {
 public:
  // Info class stores necessary information to create a clone of
  // ChildURLLoaderFactoryGetter in worker thread.
  class Info {
   public:
    Info(network::mojom::URLLoaderFactoryPtrInfo network_loader_factory_info,
         network::mojom::URLLoaderFactoryPtrInfo blob_loader_factory_info);
    Info(Info&& other);
    ~Info();

    scoped_refptr<ChildURLLoaderFactoryGetter> Bind();

   private:
    network::mojom::URLLoaderFactoryPtrInfo network_loader_factory_info_;
    network::mojom::URLLoaderFactoryPtrInfo blob_loader_factory_info_;
  };

  virtual Info GetClonedInfo() = 0;

  // Returns the URLLoaderFactory that can handle the given |request_url| (e.g.
  // returns BlobURLLoader factory for blob: URL requests). When an appropriate
  // factory cannot be determined, |default_factory| is returned if non-null
  // factory is given, or Network URLLoaderFactory is returned otherwise.
  virtual network::mojom::URLLoaderFactory* GetFactoryForURL(
      const GURL& request_url,
      network::mojom::URLLoaderFactory* default_factory = nullptr) = 0;

  virtual network::mojom::URLLoaderFactory* GetNetworkLoaderFactory() = 0;
  virtual network::mojom::URLLoaderFactory* GetBlobLoaderFactory() = 0;

 protected:
  friend class base::RefCounted<ChildURLLoaderFactoryGetter>;
  virtual ~ChildURLLoaderFactoryGetter() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CHILD_URL_LOADER_FACTORY_GETTER_H_
