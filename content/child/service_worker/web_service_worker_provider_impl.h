// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace blink {
class WebURL;
class WebServiceWorkerProviderClient;
}

namespace content {

class ServiceWorkerDispatcher;
class ServiceWorkerProviderContext;
class ThreadSafeSender;

struct ServiceWorkerVersionAttributes;

// This class corresponds to one ServiceWorkerContainer interface in
// JS context (i.e. navigator.serviceWorker).
class WebServiceWorkerProviderImpl : public blink::WebServiceWorkerProvider {
 public:
  WebServiceWorkerProviderImpl(ThreadSafeSender* thread_safe_sender,
                               ServiceWorkerProviderContext* context);
  ~WebServiceWorkerProviderImpl() override;

  void SetClient(blink::WebServiceWorkerProviderClient* client) override;

  // blink::WebServiceWorkerProvider implementation.
  void RegisterServiceWorker(
      const blink::WebURL& web_pattern,
      const blink::WebURL& web_script_url,
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks>) override;
  void GetRegistration(
      const blink::WebURL& web_document_url,
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

  void OnRegistered(
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
      const base::Optional<ServiceWorkerVersionAttributes>& attributes);

  void OnDidGetRegistration(
      std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
      const base::Optional<ServiceWorkerVersionAttributes>& attributes);

  void OnDidGetRegistrations(
      std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      base::Optional<
          std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>>
          infos,
      const base::Optional<std::vector<ServiceWorkerVersionAttributes>>& attrs);

  void OnDidGetRegistrationForReady(
      std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks>
          callbacks,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
      const base::Optional<ServiceWorkerVersionAttributes>& attributes);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<ServiceWorkerProviderContext> context_;

  base::WeakPtrFactory<WebServiceWorkerProviderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerProviderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
