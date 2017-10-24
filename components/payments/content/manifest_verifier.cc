// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/manifest_verifier.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/payments/content/payment_manifest_parser_host.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "components/webdata/common/web_data_results.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

const char* const kAllOriginsSupportedIndicator = "*";

// Enables |method_manifest_url| in those |apps| that are from |app_origins|, if
// either |all_origins_supported| is true or |supported_origin_strings| contains
// the origin of the app.
void EnableMethodManifestUrlForSupportedApps(
    const GURL& method_manifest_url,
    const std::vector<std::string>& supported_origin_strings,
    bool all_origins_supported,
    const std::set<url::Origin>& app_origins,
    content::PaymentAppProvider::PaymentApps* apps) {
  for (const auto& app_origin : app_origins) {
    const std::string app_origin_string = app_origin.Serialize();
    for (auto& app : *apps) {
      if (app_origin.IsSameOriginWith(
              url::Origin::Create(app.second->scope.GetOrigin()))) {
        if (all_origins_supported ||
            std::find(supported_origin_strings.begin(),
                      supported_origin_strings.end(),
                      app_origin_string) != supported_origin_strings.end()) {
          app.second->enabled_methods.emplace_back(method_manifest_url.spec());
        } else {
          LOG(ERROR) << "Payment handlers from \"" << app_origin
                     << "\" are not allowed to use payment method \""
                     << method_manifest_url << "\".";
        }
      }
    }
  }
}

}  // namespace

ManifestVerifier::ManifestVerifier(
    std::unique_ptr<PaymentMethodManifestDownloaderInterface> downloader,
    std::unique_ptr<PaymentManifestParserHost> parser,
    scoped_refptr<PaymentManifestWebDataService> cache)
    : downloader_(std::move(downloader)),
      parser_(std::move(parser)),
      cache_(cache),
      number_of_manifests_to_verify_(0),
      number_of_manifests_to_download_(0),
      weak_ptr_factory_(this) {
  parser_->StartUtilityProcess();
}

ManifestVerifier::~ManifestVerifier() {
  for (const auto& handle : cache_request_handles_) {
    cache_->CancelRequest(handle.first);
  }
}

void ManifestVerifier::Verify(content::PaymentAppProvider::PaymentApps apps,
                              VerifyCallback finished_verification,
                              base::OnceClosure finished_using_resources) {
  DCHECK(apps_.empty());
  DCHECK(finished_verification_callback_.is_null());
  DCHECK(finished_using_resources_callback_.is_null());

  apps_ = std::move(apps);
  finished_verification_callback_ = std::move(finished_verification);
  finished_using_resources_callback_ = std::move(finished_using_resources);

  std::set<GURL> manifests_to_download;
  for (auto& app : apps_) {
    std::vector<std::string> verified_method_names;
    for (const auto& method : app.second->enabled_methods) {
      // For non-URL payment method names, only names published by W3C should be
      // supported.
      // https://w3c.github.io/payment-method-basic-card/
      // https://w3c.github.io/webpayments/proposals/interledger-payment-method.html
      if (method == "basic-card" || method == "interledger") {
        verified_method_names.emplace_back(method);
        continue;
      }

      // All URL payment method names must be HTTPS.
      GURL method_manifest_url = GURL(method);
      if (!method_manifest_url.is_valid() ||
          method_manifest_url.scheme() != "https") {
        LOG(ERROR) << "\"" << method
                   << "\" is not a valid payment method name.";
        continue;
      }

      // Same origin payment methods are always allowed.
      url::Origin app_origin =
          url::Origin::Create(app.second->scope.GetOrigin());
      if (url::Origin::Create(method_manifest_url.GetOrigin())
              .IsSameOriginWith(app_origin)) {
        verified_method_names.emplace_back(method);
        continue;
      }

      manifests_to_download.insert(method_manifest_url);
      manifest_url_to_app_origins_map_[method_manifest_url].insert(app_origin);
    }

    app.second->enabled_methods.swap(verified_method_names);
  }

  number_of_manifests_to_verify_ = number_of_manifests_to_download_ =
      manifests_to_download.size();
  if (number_of_manifests_to_verify_ == 0) {
    RemoveInvalidPaymentApps();
    std::move(finished_verification_callback_).Run(std::move(apps_));
    std::move(finished_using_resources_callback_).Run();
    return;
  }

  for (const auto& method_manifest_url : manifests_to_download) {
    cache_request_handles_[cache_->GetPaymentMethodManifest(
        method_manifest_url.spec(), this)] = method_manifest_url;
  }
}

void ManifestVerifier::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_LT(0U, number_of_manifests_to_verify_);

  auto it = cache_request_handles_.find(h);
  if (it == cache_request_handles_.end())
    return;

  GURL method_manifest_url = it->second;
  cache_request_handles_.erase(it);

  const std::vector<std::string>& supported_origin_strings =
      (static_cast<const WDResult<std::vector<std::string>>*>(result.get()))
          ->GetValue();
  bool all_origins_supported = std::find(supported_origin_strings.begin(),
                                         supported_origin_strings.end(),
                                         kAllOriginsSupportedIndicator) !=
                               supported_origin_strings.end();
  EnableMethodManifestUrlForSupportedApps(
      method_manifest_url, supported_origin_strings, all_origins_supported,
      manifest_url_to_app_origins_map_[method_manifest_url], &apps_);

  if (!supported_origin_strings.empty()) {
    cached_manifest_urls_.insert(method_manifest_url);
    if (--number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_).Run(std::move(apps_));
    }
  }

  downloader_->DownloadPaymentMethodManifest(
      method_manifest_url,
      base::BindOnce(&ManifestVerifier::OnPaymentMethodManifestDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void ManifestVerifier::OnPaymentMethodManifestDownloaded(
    const GURL& method_manifest_url,
    const std::string& content) {
  DCHECK_LT(0U, number_of_manifests_to_download_);

  if (content.empty()) {
    if (cached_manifest_urls_.find(method_manifest_url) ==
            cached_manifest_urls_.end() &&
        --number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_).Run(std::move(apps_));
    }

    if (--number_of_manifests_to_download_ == 0)
      std::move(finished_using_resources_callback_).Run();

    return;
  }

  parser_->ParsePaymentMethodManifest(
      content,
      base::BindOnce(&ManifestVerifier::OnPaymentMethodManifestParsed,
                     weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void ManifestVerifier::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
    const std::vector<GURL>& default_applications,
    const std::vector<url::Origin>& supported_origins,
    bool all_origins_supported) {
  DCHECK_LT(0U, number_of_manifests_to_download_);

  std::vector<std::string> supported_origin_strings(supported_origins.size());
  std::transform(supported_origins.begin(), supported_origins.end(),
                 supported_origin_strings.begin(),
                 [](const auto& origin) { return origin.Serialize(); });

  if (cached_manifest_urls_.find(method_manifest_url) ==
      cached_manifest_urls_.end()) {
    EnableMethodManifestUrlForSupportedApps(
        method_manifest_url, supported_origin_strings, all_origins_supported,
        manifest_url_to_app_origins_map_[method_manifest_url], &apps_);

    if (--number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_).Run(std::move(apps_));
    }
  }

  if (all_origins_supported)
    supported_origin_strings.emplace_back(kAllOriginsSupportedIndicator);
  cache_->AddPaymentMethodManifest(method_manifest_url.spec(),
                                   supported_origin_strings);

  if (--number_of_manifests_to_download_ == 0)
    std::move(finished_using_resources_callback_).Run();
}

void ManifestVerifier::RemoveInvalidPaymentApps() {
  // Remove apps without enabled methods.
  std::vector<int64_t> keys_to_erase;
  for (const auto& app : apps_) {
    if (app.second->enabled_methods.empty())
      keys_to_erase.emplace_back(app.first);
  }

  for (const auto& key : keys_to_erase) {
    apps_.erase(key);
  }
}

}  // namespace payments
