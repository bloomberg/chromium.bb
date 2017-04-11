// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_parser_host.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace payments {

PaymentManifestParserHost::PaymentManifestParserHost() : callback_counter_(0) {}

PaymentManifestParserHost::~PaymentManifestParserHost() {}

void PaymentManifestParserHost::StartUtilityProcess() {
  mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<mojom::PaymentManifestParser>>(
      l10n_util::GetStringUTF16(
          IDS_UTILITY_PROCESS_PAYMENT_MANIFEST_PARSER_NAME));
  mojo_client_->set_error_callback(
      base::Bind(&PaymentManifestParserHost::OnUtilityProcessStopped,
                 base::Unretained(this)));
  mojo_client_->Start();
}

void PaymentManifestParserHost::ParsePaymentMethodManifest(
    const std::string& content,
    PaymentMethodCallback callback) {
  if (!mojo_client_) {
    std::move(callback).Run(std::vector<GURL>());
    return;
  }

  int64_t callback_identifier = callback_counter_++;
  const auto& result = pending_payment_method_callbacks_.insert(
      std::make_pair(callback_identifier, std::move(callback)));
  DCHECK(result.second);
  DCHECK_GE(10U, pending_payment_method_callbacks_.size());

  mojo_client_->service()->ParsePaymentMethodManifest(
      content, base::Bind(&PaymentManifestParserHost::OnPaymentMethodParse,
                          base::Unretained(this), callback_identifier));
}

void PaymentManifestParserHost::ParseWebAppManifest(const std::string& content,
                                                    WebAppCallback callback) {
  if (!mojo_client_) {
    std::move(callback).Run(std::vector<mojom::WebAppManifestSectionPtr>());
    return;
  }

  int64_t callback_identifier = callback_counter_++;
  const auto& result = pending_web_app_callbacks_.insert(
      std::make_pair(callback_identifier, std::move(callback)));
  DCHECK(result.second);
  DCHECK_GE(10U, pending_web_app_callbacks_.size());

  mojo_client_->service()->ParseWebAppManifest(
      content, base::Bind(&PaymentManifestParserHost::OnWebAppParse,
                          base::Unretained(this), callback_identifier));
}

void PaymentManifestParserHost::OnPaymentMethodParse(
    int64_t callback_identifier,
    const std::vector<GURL>& web_app_manifest_urls) {
  const auto& pending_callback_it =
      pending_payment_method_callbacks_.find(callback_identifier);
  if (pending_callback_it == pending_payment_method_callbacks_.end()) {
    // If unable to find the pending callback, then something went wrong in the
    // utility process. Stop the utility process and notify all callbacks.
    OnUtilityProcessStopped();
    return;
  }

  const size_t kMaximumNumberOfWebAppUrls = 100U;
  if (web_app_manifest_urls.size() > kMaximumNumberOfWebAppUrls) {
    // If more than 100 items, then something went wrong in the utility
    // process. Stop the utility process and notify all callbacks.
    OnUtilityProcessStopped();
    return;
  }

  for (const auto& url : web_app_manifest_urls) {
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      // If not a valid URL with HTTPS scheme, then something went wrong in the
      // utility process. Stop the utility process and notify all callbacks.
      OnUtilityProcessStopped();
      return;
    }
  }

  PaymentMethodCallback callback = std::move(pending_callback_it->second);
  pending_payment_method_callbacks_.erase(pending_callback_it);

  // Can trigger synchronous deletion of this object, so can't access any of
  // the member variables after this block.
  std::move(callback).Run(web_app_manifest_urls);
}

void PaymentManifestParserHost::OnWebAppParse(
    int64_t callback_identifier,
    std::vector<mojom::WebAppManifestSectionPtr> manifest) {
  const auto& pending_callback_it =
      pending_web_app_callbacks_.find(callback_identifier);
  if (pending_callback_it == pending_web_app_callbacks_.end()) {
    // If unable to find the pending callback, then something went wrong in the
    // utility process. Stop the utility process and notify all callbacks.
    OnUtilityProcessStopped();
    return;
  }

  const size_t kMaximumNumberOfSections = 100U;
  if (manifest.size() > kMaximumNumberOfSections) {
    // If more than 100 items, then something went wrong in the utility
    // process. Stop the utility process and notify all callbacks.
    OnUtilityProcessStopped();
    return;
  }

  for (size_t i = 0; i < manifest.size(); ++i) {
    const size_t kMaximumNumberOfFingerprints = 100U;
    if (manifest[i]->fingerprints.size() > kMaximumNumberOfFingerprints) {
      // If more than 100 items, then something went wrong in the utility
      // process. Stop the utility process and notify all callbacks.
      OnUtilityProcessStopped();
      return;
    }
  }

  WebAppCallback callback = std::move(pending_callback_it->second);
  pending_web_app_callbacks_.erase(pending_callback_it);

  // Can trigger synchronous deletion of this object, so can't access any of
  // the member variables after this block.
  std::move(callback).Run(std::move(manifest));
}

void PaymentManifestParserHost::OnUtilityProcessStopped() {
  mojo_client_.reset();

  std::unordered_map<int64_t, PaymentMethodCallback> payment_method_callbacks =
      std::move(pending_payment_method_callbacks_);
  std::unordered_map<int64_t, WebAppCallback> web_app_callbacks =
      std::move(pending_web_app_callbacks_);

  for (auto& callback : payment_method_callbacks) {
    // Can trigger synchronous deletion of this object, so can't access any of
    // the member variables after this line.
    std::move(callback.second).Run(std::vector<GURL>());
  }

  for (auto& callback : web_app_callbacks) {
    // Can trigger synchronous deletion of this object, so can't access any of
    // the member variables after this line.
    std::move(callback.second)
        .Run(std::vector<mojom::WebAppManifestSectionPtr>());
  }
}

}  // namespace payments
