// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/payment_app_provider.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"

template <class T>
class scoped_refptr;

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

class ManifestVerifier;
class PaymentManifestWebDataService;
class PaymentMethodManifestDownloaderInterface;

// Retrieves service worker payment apps.
class ServiceWorkerPaymentAppFactory {
 public:
  ServiceWorkerPaymentAppFactory();
  ~ServiceWorkerPaymentAppFactory();

  // Retrieves all service worker payment apps that can handle payments for
  // |requested_method_data|, verifies these apps are allowed to handle these
  // payment methods, and filters them by their capabilities and
  // CanMakePaymentEvent responses.
  //
  // This method takes ownership of |downloader|. (Passing the |downloader| into
  // this method is useful for testing.)
  //
  // The payment apps will be returned through |callback|. After |callback| has
  // been invoked, it's safe to show the apps in UI for user to select one of
  // these apps for payment.
  //
  // After |callback| has fired, the factory refreshes its own cache in the
  // background. Once the cache has been refreshed, the factory invokes the
  // |finished_using_resources_callback|. At this point, it's safe to delete
  // this factory. Don't destroy the factory and don't call this method again
  // until |finished_using_resources_callback| has run.
  //
  // The method should be called on the UI thread.
  void GetAllPaymentApps(
      content::BrowserContext* browser_context,
      std::unique_ptr<PaymentMethodManifestDownloaderInterface> downloader,
      scoped_refptr<PaymentManifestWebDataService> cache,
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::PaymentAppProvider::GetAllPaymentAppsCallback callback,
      base::OnceClosure finished_using_resources_callback);

 private:
  friend class ServiceWorkerPaymentAppFactoryBrowserTest;
  friend class ServiceWorkerPaymentAppFactoryTest;

  // Removes |apps| that don't match any of the |requested_method_data| based on
  // the method names and method-specific capabilities.
  static void RemoveAppsWithoutMatchingMethodData(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::PaymentAppProvider::PaymentApps* apps);

  // Should be used only in tests.
  void IgnorePortInAppScopeForTesting();

  void OnGotAllPaymentApps(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::PaymentAppProvider::GetAllPaymentAppsCallback callback,
      base::OnceClosure finished_using_resources_callback,
      content::PaymentAppProvider::PaymentApps apps);

  void OnPaymentAppsVerifierFinishedUsingResources(
      base::OnceClosure finished_verification_callback);

  std::unique_ptr<ManifestVerifier> verifier_;
  bool ignore_port_in_app_scope_for_testing_ = false;
  base::WeakPtrFactory<ServiceWorkerPaymentAppFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentAppFactory);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FACTORY_H_
