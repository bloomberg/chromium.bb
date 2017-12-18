// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

class StoragePartitionImpl;

// Holds on to URLLoaderFactory for a given StoragePartition and allows code
// running on the IO thread to access them. Note these are the factories used by
// the browser process for frame requests.
class URLLoaderFactoryGetter
    : public base::RefCountedThreadSafe<URLLoaderFactoryGetter,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  CONTENT_EXPORT URLLoaderFactoryGetter();

  // Initializes this object on the UI thread. The |partition| is used to
  // initialize the URLLoaderFactories for the network service and AppCache.
  void Initialize(StoragePartitionImpl* partition);

  // Called on the IO thread to get the URLLoaderFactory to the network service.
  // The pointer shouldn't be cached.
  mojom::URLLoaderFactory* GetNetworkFactory();

  // Called on the IO thread to get the URLLoaderFactory to the blob service.
  // The pointer shouldn't be cached.
  CONTENT_EXPORT mojom::URLLoaderFactory* GetBlobFactory();

  // Overrides the network URLLoaderFactory for subsequent requests. Passing a
  // null pointer will restore the default behavior.
  CONTENT_EXPORT void SetNetworkFactoryForTesting(
      mojom::URLLoaderFactory* test_factory);

  CONTENT_EXPORT mojom::URLLoaderFactoryPtr*
  original_network_factory_for_testing() {
    return &network_factory_;
  }

  // When this global function is set, if GetNetworkFactory is called and
  // |test_factory_| is null, then the callback will be run.
  // This method must be called either on the IO thread or before threads start.
  // This callback is run on the IO thread.
  using GetNetworkFactoryCallback = base::RepeatingCallback<void(
      URLLoaderFactoryGetter* url_loader_factory_getter)>;
  CONTENT_EXPORT static void SetGetNetworkFactoryCallbackForTesting(
      const GetNetworkFactoryCallback& get_network_factory_callback);

 private:
  friend class base::DeleteHelper<URLLoaderFactoryGetter>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  CONTENT_EXPORT ~URLLoaderFactoryGetter();
  void InitializeOnIOThread(mojom::URLLoaderFactoryPtrInfo network_factory,
                            mojom::URLLoaderFactoryPtrInfo blob_factory);

  // Only accessed on IO thread.
  mojom::URLLoaderFactoryPtr network_factory_;
  mojom::URLLoaderFactoryPtr blob_factory_;
  mojom::URLLoaderFactory* test_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryGetter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
