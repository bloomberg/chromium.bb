// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"

namespace blink {
class WebURL;
class WebServiceWorkerProviderClient;
}

namespace content {

class ServiceWorkerDispatcher;
class ServiceWorkerProviderContext;
class ThreadSafeSender;

// This class corresponds to one ServiceWorkerContainer interface in
// JS context (i.e. navigator.serviceWorker).
class WebServiceWorkerProviderImpl : public blink::WebServiceWorkerProvider {
 public:
  WebServiceWorkerProviderImpl(ThreadSafeSender* thread_safe_sender,
                               ServiceWorkerProviderContext* context);
  ~WebServiceWorkerProviderImpl() override;

  void SetClient(blink::WebServiceWorkerProviderClient* client) override;

  void RegisterServiceWorker(
      const blink::WebURL& pattern,
      const blink::WebURL& script_url,
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks>) override;
  void GetRegistration(
      const blink::WebURL& document_url,
      std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks>) override;
  void GetRegistrations(
      std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks>) override;
  void GetRegistrationForReady(
      std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks>)
      override;
  bool ValidateScopeAndScriptURL(const blink::WebURL& pattern,
                                 const blink::WebURL& script_url,
                                 blink::WebString* error_message) override;

  int provider_id() const;

 private:
  void RemoveProviderClient();
  ServiceWorkerDispatcher* GetDispatcher();

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<ServiceWorkerProviderContext> context_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerProviderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
