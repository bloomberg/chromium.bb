// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/mojom/payment_app.mojom.h"
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
      base::OnceCallback<void(payments::mojom::PaymentAppManifestError)>;
  using ReadManifestCallback =
      base::OnceCallback<void(payments::mojom::PaymentAppManifestPtr,
                              payments::mojom::PaymentAppManifestError)>;
  using ManifestWithID =
      std::pair<int64_t, payments::mojom::PaymentAppManifestPtr>;
  using Manifests = std::vector<ManifestWithID>;
  using ReadAllManifestsCallback = base::OnceCallback<void(Manifests)>;
  using DeletePaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using ReadPaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentInstrumentPtr,
                              payments::mojom::PaymentHandlerStatus)>;
  using KeysOfPaymentInstrumentsCallback =
      base::OnceCallback<void(const std::vector<std::string>&,
                              payments::mojom::PaymentHandlerStatus)>;
  using HasPaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using WritePaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using ClearPaymentInstrumentsCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;

  explicit PaymentAppDatabase(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~PaymentAppDatabase();

  void WriteManifest(const GURL& scope,
                     payments::mojom::PaymentAppManifestPtr manifest,
                     WriteManifestCallback callback);
  void ReadManifest(const GURL& scope, ReadManifestCallback callback);
  void ReadAllManifests(ReadAllManifestsCallback callback);
  void DeletePaymentInstrument(const GURL& scope,
                               const std::string& instrument_key,
                               DeletePaymentInstrumentCallback callback);
  void ReadPaymentInstrument(const GURL& scope,
                             const std::string& instrument_key,
                             ReadPaymentInstrumentCallback callback);
  void KeysOfPaymentInstruments(const GURL& scope,
                                KeysOfPaymentInstrumentsCallback callback);
  void HasPaymentInstrument(const GURL& scope,
                            const std::string& instrument_key,
                            HasPaymentInstrumentCallback callback);
  void WritePaymentInstrument(const GURL& scope,
                              const std::string& instrument_key,
                              payments::mojom::PaymentInstrumentPtr instrument,
                              WritePaymentInstrumentCallback callback);
  void ClearPaymentInstruments(const GURL& scope,
                               ClearPaymentInstrumentsCallback callback);

 private:
  // WriteManifest callbacks
  void DidFindRegistrationToWriteManifest(
      payments::mojom::PaymentAppManifestPtr manifest,
      WriteManifestCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidWriteManifest(WriteManifestCallback callback,
                        ServiceWorkerStatusCode status);

  // ReadManifest callbacks
  void DidFindRegistrationToReadManifest(
      ReadManifestCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidReadManifest(ReadManifestCallback callback,
                       const std::vector<std::string>& data,
                       ServiceWorkerStatusCode status);

  // ReadAllManifests callbacks
  void DidReadAllManifests(
      ReadAllManifestsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& raw_data,
      ServiceWorkerStatusCode status);

  // DeletePaymentInstrument callbacks
  void DidFindRegistrationToDeletePaymentInstrument(
      const std::string& instrument_key,
      DeletePaymentInstrumentCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindPaymentInstrument(int64_t registration_id,
                                const std::string& instrument_key,
                                DeletePaymentInstrumentCallback callback,
                                const std::vector<std::string>& data,
                                ServiceWorkerStatusCode status);
  void DidDeletePaymentInstrument(DeletePaymentInstrumentCallback callback,
                                  ServiceWorkerStatusCode status);

  // ReadPaymentInstrument callbacks
  void DidFindRegistrationToReadPaymentInstrument(
      const std::string& instrument_key,
      ReadPaymentInstrumentCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidReadPaymentInstrument(ReadPaymentInstrumentCallback callback,
                                const std::vector<std::string>& data,
                                ServiceWorkerStatusCode status);

  // KeysOfPaymentInstruments callbacks
  void DidFindRegistrationToGetKeys(
      KeysOfPaymentInstrumentsCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetKeysOfPaymentInstruments(KeysOfPaymentInstrumentsCallback callback,
                                      const std::vector<std::string>& data,
                                      ServiceWorkerStatusCode status);

  // HasPaymentInstrument callbacks
  void DidFindRegistrationToHasPaymentInstrument(
      const std::string& instrument_key,
      HasPaymentInstrumentCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidHasPaymentInstrument(int64_t registration_id,
                               const std::string& instrument_key,
                               DeletePaymentInstrumentCallback callback,
                               const std::vector<std::string>& data,
                               ServiceWorkerStatusCode status);

  // WritePaymentInstrument callbacks
  void DidFindRegistrationToWritePaymentInstrument(
      const std::string& instrument_key,
      payments::mojom::PaymentInstrumentPtr instrument,
      WritePaymentInstrumentCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidWritePaymentInstrument(WritePaymentInstrumentCallback callback,
                                 ServiceWorkerStatusCode status);

  // ClearPaymentInstruments callbacks
  void DidFindRegistrationToClearPaymentInstruments(
      const GURL& scope,
      ClearPaymentInstrumentsCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetKeysToClearPaymentInstruments(
      int64_t registration_id,
      ClearPaymentInstrumentsCallback callback,
      const std::vector<std::string>& keys,
      payments::mojom::PaymentHandlerStatus status);
  void DidClearPaymentInstruments(ClearPaymentInstrumentsCallback callback,
                                  ServiceWorkerStatusCode status);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<PaymentAppDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
