// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#define CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace content {
class ResourceContext;

struct ChromeAppCacheServiceDeleter;

// An AppCacheServiceImpl subclass used by the chrome. There is an instance
// associated with each BrowserContext. This derivation adds refcounting
// semantics since a browser context has multiple URLRequestContexts which refer
// to the same object, and those URLRequestContexts are refcounted independently
// of the owning browser context.
//
// All methods, except the ctor, are expected to be called on
// the IO thread (unless specifically called out in doc comments).
//
// TODO(dpranke): Fix dependencies on AppCacheServiceImpl so that we don't have
// to worry about clients calling AppCacheServiceImpl methods.
class CONTENT_EXPORT ChromeAppCacheService
    : public base::RefCountedThreadSafe<ChromeAppCacheService,
                                        ChromeAppCacheServiceDeleter>,
      public AppCacheServiceImpl,
      public AppCachePolicy {
 public:
  explicit ChromeAppCacheService(storage::QuotaManagerProxy* proxy);

  // If |cache_path| is empty we will use in-memory structs.
  void InitializeOnIOThread(
      const base::FilePath& cache_path,
      ResourceContext* resource_context,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  void Bind(std::unique_ptr<blink::mojom::AppCacheBackend> backend,
            blink::mojom::AppCacheBackendRequest request,
            int process_id);
  // Unbinds the pipe corresponding to the given process_id. Unbinding
  // unregisters and destroys the existing backend for that process_id.
  // The function must be called before a new backend is created for the given
  // process_id to ensure that there is at most one backend per process_id.
  // The function does nothing if no pipe was bound.
  void Unbind(int process_id);

  void Shutdown();

  // AppCachePolicy overrides
  bool CanLoadAppCache(const GURL& manifest_url,
                       const GURL& first_party) override;
  bool CanCreateAppCache(const GURL& manifest_url,
                         const GURL& first_party) override;

  // AppCacheServiceImpl override
  void UnregisterBackend(AppCacheBackendImpl* backend_impl) override;

 protected:
  ~ChromeAppCacheService() override;

 private:
  friend class base::DeleteHelper<ChromeAppCacheService>;
  friend class base::RefCountedThreadSafe<ChromeAppCacheService,
                                          ChromeAppCacheServiceDeleter>;
  friend struct ChromeAppCacheServiceDeleter;

  void DeleteOnCorrectThread() const;

  ResourceContext* resource_context_;
  base::FilePath cache_path_;
  mojo::StrongBindingSet<blink::mojom::AppCacheBackend> bindings_;

  // A map from a process_id to a binding_id.
  std::map<int, mojo::BindingId> process_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppCacheService);
};

struct ChromeAppCacheServiceDeleter {
  static void Destruct(const ChromeAppCacheService* service) {
    service->DeleteOnCorrectThread();
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
