// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/payment_app.mojom.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class PaymentAppContext;
class ServiceWorkerRegistration;

class CONTENT_EXPORT PaymentAppManager
    : public NON_EXPORTED_BASE(payments::mojom::PaymentAppManager) {
 public:
  PaymentAppManager(
      PaymentAppContext* payment_app_context,
      mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request);

  ~PaymentAppManager() override;

 private:
  friend class PaymentAppManagerTest;

  // payments::mojom::PaymentAppManager methods:
  void SetManifest(const std::string& scope,
                   payments::mojom::PaymentAppManifestPtr manifest,
                   const SetManifestCallback& callback) override;
  void GetManifest(const std::string& scope,
                   const GetManifestCallback& callback) override;

  // SetManifest callbacks
  void DidFindRegistrationToSetManifest(
      payments::mojom::PaymentAppManifestPtr manifest,
      const SetManifestCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidSetManifest(const SetManifestCallback& callback,
                      ServiceWorkerStatusCode status);

  // GetManifest callbacks
  void DidFindRegistrationToGetManifest(
      const GetManifestCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetManifest(const GetManifestCallback& callback,
                      const std::vector<std::string>& data,
                      ServiceWorkerStatusCode status);

  // Called when an error is detected on binding_.
  void OnConnectionError();

  // PaymentAppContext owns PaymentAppManager
  PaymentAppContext* payment_app_context_;

  mojo::Binding<payments::mojom::PaymentAppManager> binding_;

  base::WeakPtrFactory<PaymentAppManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_
