// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include <stdint.h>
#include <memory>

#include "content/browser/appcache/appcache_host.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

class AppCacheServiceImpl;

class CONTENT_EXPORT AppCacheBackendImpl
    : public blink::mojom::AppCacheBackend {
 public:
  AppCacheBackendImpl(AppCacheServiceImpl* service, int process_id);
  ~AppCacheBackendImpl() override;

  int process_id() const { return process_id_; }

  // blink::mojom::AppCacheBackend
  void RegisterHost(blink::mojom::AppCacheHostRequest host_request,
                    blink::mojom::AppCacheFrontendPtr frontend,
                    const base::UnguessableToken& host_id) override;

 private:
  // Raw pointer is safe because instances of this class are owned by
  // |service_|.
  AppCacheServiceImpl* service_;
  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheBackendImpl);
};

}  // namespace

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
