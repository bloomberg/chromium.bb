// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class StoragePartitionImpl;

// Holds on to URLLoaderFactory for a given StoragePartition and allows code
// running on the IO thread to access them. Note these are the factories used by
// the browser process for frame requests.
class URLLoaderFactoryGetter
    : public base::RefCountedThreadSafe<URLLoaderFactoryGetter,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  URLLoaderFactoryGetter();

  // Initializes this object on the UI thread. The |partition| is used to
  // initialize the URLLoaderFactories for the network service and AppCache.
  void Initialize(StoragePartitionImpl* partition);

  // Called on the IO thread to get the URLLoaderFactory to the network service.
  // The pointer shouldn't be cached.
  mojom::URLLoaderFactoryPtr* GetNetworkFactory();

  // Called on the IO thread to get the URLLoaderFactory to the blob service.
  // The pointer shouldn't be cached.
  CONTENT_EXPORT mojom::URLLoaderFactoryPtr* GetBlobFactory();

  // Overrides the network URLLoaderFactory for subsequent requests. Passing a
  // null pointer will restore the default behavior.
  // This is called on the UI thread.
  CONTENT_EXPORT void SetNetworkFactoryForTesting(
      mojom::URLLoaderFactoryPtr test_factory);

 private:
  friend class base::DeleteHelper<URLLoaderFactoryGetter>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  CONTENT_EXPORT ~URLLoaderFactoryGetter();
  void InitializeOnIOThread(mojom::URLLoaderFactoryPtrInfo network_factory,
                            mojom::URLLoaderFactoryPtrInfo blob_factory);
  void SetTestNetworkFactoryOnIOThread(
      mojom::URLLoaderFactoryPtrInfo test_factory);

  // Only accessed on IO thread.
  mojom::URLLoaderFactoryPtr network_factory_;
  mojom::URLLoaderFactoryPtr blob_factory_;
  mojom::URLLoaderFactoryPtr test_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryGetter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
