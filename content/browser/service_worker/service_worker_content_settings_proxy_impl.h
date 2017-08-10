// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextCore;

class ServiceWorkerContentSettingsProxyImpl final
    : public blink::mojom::WorkerContentSettingsProxy {
 public:
  ~ServiceWorkerContentSettingsProxyImpl() override;

  // Creates ServiceWorkerContentSettingsProxyImpl and binds it to |request|.
  // This creates a StrongBinding, so the lifetime of the new
  // ServiceWorkerContentSettingsProxyImpl is the same as its Mojo connection.
  static void Create(GURL script_url,
                     base::WeakPtr<ServiceWorkerContextCore> context,
                     blink::mojom::WorkerContentSettingsProxyRequest request);

  // blink::mojom::WorkerContentSettingsProxy implementation
  void AllowIndexedDB(const url::Origin& origin,
                      const base::string16& name,
                      AllowIndexedDBCallback callback) override;
  void RequestFileSystemAccessSync(
      const url::Origin& origin,
      RequestFileSystemAccessSyncCallback callback) override;

 private:
  ServiceWorkerContentSettingsProxyImpl(
      url::Origin origin,
      base::WeakPtr<ServiceWorkerContextCore> context);

  const url::Origin origin_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
