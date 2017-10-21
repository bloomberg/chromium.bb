// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "third_party/WebKit/public/platform/WebWorkerFetchContext.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {
class ResourceDispatcher;

class ServiceWorkerFetchContextImpl : public blink::WebWorkerFetchContext {
 public:
  ServiceWorkerFetchContextImpl(
      const GURL& worker_script_url,
      ChildURLLoaderFactoryGetter::Info url_loader_factory_getter_info,
      int service_worker_provider_id);
  ~ServiceWorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  void InitializeOnWorkerThread(
      scoped_refptr<base::SingleThreadTaskRunner>) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void WillSendRequest(blink::WebURLRequest&) override;
  bool IsControlledByServiceWorker() const override;
  void SetDataSaverEnabled(bool enabled) override;
  bool IsDataSaverEnabled() const override;
  blink::WebURL SiteForCookies() const override;

 private:
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |url_loader_factory_getter_|.
  ChildURLLoaderFactoryGetter::Info url_loader_factory_getter_info_;
  const int service_worker_provider_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<ChildURLLoaderFactoryGetter> url_loader_factory_getter_;

  bool is_data_saver_enabled_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
