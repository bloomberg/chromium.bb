// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class ServiceWorkerRegistration;

class CONTENT_EXPORT PaymentAppDatabase {
 public:
  using WriteManifestCallback =
      base::Callback<void(payments::mojom::PaymentAppManifestError)>;
  using ReadManifestCallback =
      base::Callback<void(payments::mojom::PaymentAppManifestPtr,
                          payments::mojom::PaymentAppManifestError)>;
  using ManifestWithID =
      std::pair<int64_t, payments::mojom::PaymentAppManifestPtr>;
  using Manifests = std::vector<ManifestWithID>;
  using ReadAllManifestsCallback = base::Callback<void(Manifests)>;

  explicit PaymentAppDatabase(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~PaymentAppDatabase();

  void WriteManifest(const GURL& scope,
                     payments::mojom::PaymentAppManifestPtr manifest,
                     const WriteManifestCallback& callback);
  void ReadManifest(const GURL& scope, const ReadManifestCallback& callback);
  void ReadAllManifests(const ReadAllManifestsCallback& callback);

 private:
  // WriteManifest callbacks
  void DidFindRegistrationToWriteManifest(
      payments::mojom::PaymentAppManifestPtr manifest,
      const WriteManifestCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidWriteManifest(const WriteManifestCallback& callback,
                        ServiceWorkerStatusCode status);

  // ReadManifest callbacks
  void DidFindRegistrationToReadManifest(
      const ReadManifestCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidReadManifest(const ReadManifestCallback& callback,
                       const std::vector<std::string>& data,
                       ServiceWorkerStatusCode status);

  // ReadAllManifests callbacks
  void DidReadAllManifests(
      const ReadAllManifestsCallback& callback,
      const std::vector<std::pair<int64_t, std::string>>& raw_data,
      ServiceWorkerStatusCode status);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<PaymentAppDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
