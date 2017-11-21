// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_factory.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/payments/content/manifest_verifier.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/url_canon.h"

namespace payments {
namespace {

// Returns true if |requested| is empty or contains at least one of the items in
// |capabilities|.
template <typename T>
bool CapabilityMatches(const std::vector<T>& requested,
                       const std::vector<int32_t>& capabilities) {
  if (requested.empty())
    return true;
  for (const auto& request : requested) {
    if (base::ContainsValue(capabilities, static_cast<int32_t>(request)))
      return true;
  }
  return false;
}

// Returns true if the basic-card |capabilities| of the payment app match the
// |request|.
bool BasicCardCapabilitiesMatch(
    const std::vector<content::StoredCapabilities>& capabilities,
    const mojom::PaymentMethodDataPtr& request) {
  for (const auto& capability : capabilities) {
    if (CapabilityMatches(request->supported_networks,
                          capability.supported_card_networks) &&
        CapabilityMatches(request->supported_types,
                          capability.supported_card_types)) {
      return true;
    }
  }
  return capabilities.empty() && request->supported_networks.empty() &&
         request->supported_types.empty();
}

// Returns true if |app| supports at least one of the |requests|.
bool AppSupportsAtLeastOneRequestedMethodData(
    const content::StoredPaymentApp& app,
    const std::vector<mojom::PaymentMethodDataPtr>& requests) {
  for (const auto& enabled_method : app.enabled_methods) {
    for (const auto& request : requests) {
      auto it = std::find(request->supported_methods.begin(),
                          request->supported_methods.end(), enabled_method);
      if (it != request->supported_methods.end()) {
        if (enabled_method != "basic-card" ||
            BasicCardCapabilitiesMatch(app.capabilities, request)) {
          return true;
        }
      }
    }
  }
  return false;
}

void RemovePortNumbersFromScopesForTest(
    content::PaymentAppProvider::PaymentApps* apps) {
  url::Replacements<char> replacements;
  replacements.ClearPort();
  for (auto& app : *apps) {
    app.second->scope = app.second->scope.ReplaceComponents(replacements);
  }
}

class SelfDeletingServiceWorkerPaymentAppFactory {
 public:
  SelfDeletingServiceWorkerPaymentAppFactory() {}
  ~SelfDeletingServiceWorkerPaymentAppFactory() {}

  // After |callback| has fired, the factory refreshes its own cache in the
  // background. Once the cache has been refreshed, the factory invokes the
  // |finished_using_resources_callback|. At this point, it's safe to delete
  // this factory. Don't destroy the factory and don't call this method again
  // until |finished_using_resources_callback| has run.
  void GetAllPaymentApps(
      content::WebContents* web_contents,
      std::unique_ptr<PaymentMethodManifestDownloaderInterface> downloader,
      scoped_refptr<PaymentManifestWebDataService> cache,
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::PaymentAppProvider::GetAllPaymentAppsCallback callback,
      base::OnceClosure finished_using_resources_callback) {
    DCHECK(!verifier_);

    verifier_ = std::make_unique<ManifestVerifier>(
        web_contents, std::move(downloader),
        std::make_unique<PaymentManifestParser>(), cache);

    // Method data cannot be copied and is passed in as a const-ref, which
    // cannot be moved, so make a manual copy for moving into the callback
    // below.
    std::vector<mojom::PaymentMethodDataPtr> requested_method_data_copy;
    for (const auto& request : requested_method_data) {
      requested_method_data_copy.emplace_back(request.Clone());
    }

    content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
        web_contents->GetBrowserContext(),
        base::BindOnce(
            &SelfDeletingServiceWorkerPaymentAppFactory::OnGotAllPaymentApps,
            base::Unretained(this), std::move(requested_method_data_copy),
            std::move(callback),
            base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFactory::
                               OnPaymentAppsVerifierFinishedUsingResources,
                           base::Owned(this),
                           std::move(finished_using_resources_callback))));
  }

  void IgnorePortInAppScopeForTesting() {
    ignore_port_in_app_scope_for_testing_ = true;
  }

 private:
  void OnGotAllPaymentApps(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::PaymentAppProvider::GetAllPaymentAppsCallback callback,
      base::OnceClosure finished_using_resources_callback,
      content::PaymentAppProvider::PaymentApps apps) {
    if (ignore_port_in_app_scope_for_testing_)
      RemovePortNumbersFromScopesForTest(&apps);

    ServiceWorkerPaymentAppFactory::RemoveAppsWithoutMatchingMethodData(
        requested_method_data, &apps);
    if (apps.empty()) {
      std::move(callback).Run(std::move(apps));
      std::move(finished_using_resources_callback).Run();
      return;
    }

    // The |verifier_| will invoke |callback| with the list of all valid payment
    // apps. This list may be empty, if none of the apps were found to be valid.
    verifier_->Verify(std::move(apps), std::move(callback),
                      std::move(finished_using_resources_callback));
  }

  void OnPaymentAppsVerifierFinishedUsingResources(
      base::OnceClosure finished_using_resources_callback) {
    verifier_.reset();
    std::move(finished_using_resources_callback).Run();
    // No need to self-delete here, because of using base::Owned(this).
  }

  std::unique_ptr<ManifestVerifier> verifier_;
  bool ignore_port_in_app_scope_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(SelfDeletingServiceWorkerPaymentAppFactory);
};

}  // namespace

// static
ServiceWorkerPaymentAppFactory* ServiceWorkerPaymentAppFactory::GetInstance() {
  return base::Singleton<ServiceWorkerPaymentAppFactory>::get();
}

void ServiceWorkerPaymentAppFactory::GetAllPaymentApps(
    content::WebContents* web_contents,
    scoped_refptr<PaymentManifestWebDataService> cache,
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    content::PaymentAppProvider::GetAllPaymentAppsCallback callback,
    base::OnceClosure finished_writing_cache_callback_for_testing) {
  SelfDeletingServiceWorkerPaymentAppFactory* self_delete_factory =
      new SelfDeletingServiceWorkerPaymentAppFactory();
  if (test_downloader_ != nullptr)
    self_delete_factory->IgnorePortInAppScopeForTesting();

  self_delete_factory->GetAllPaymentApps(
      web_contents,
      test_downloader_ == nullptr
          ? std::make_unique<payments::PaymentManifestDownloader>(
                content::BrowserContext::GetDefaultStoragePartition(
                    web_contents->GetBrowserContext())
                    ->GetURLRequestContext())
          : std::move(test_downloader_),
      cache, requested_method_data, std::move(callback),
      std::move(finished_writing_cache_callback_for_testing));
}

ServiceWorkerPaymentAppFactory::ServiceWorkerPaymentAppFactory()
    : test_downloader_(nullptr) {}

ServiceWorkerPaymentAppFactory::~ServiceWorkerPaymentAppFactory() {}

// static
void ServiceWorkerPaymentAppFactory::RemoveAppsWithoutMatchingMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    content::PaymentAppProvider::PaymentApps* apps) {
  for (auto it = apps->begin(); it != apps->end();) {
    if (AppSupportsAtLeastOneRequestedMethodData(*it->second,
                                                 requested_method_data)) {
      ++it;
    } else {
      it = apps->erase(it);
    }
  }
}

void ServiceWorkerPaymentAppFactory::
    SetDownloaderAndIgnorePortInAppScopeForTesting(
        std::unique_ptr<PaymentMethodManifestDownloaderInterface> downloader) {
  test_downloader_ = std::move(downloader);
}

}  // namespace payments
