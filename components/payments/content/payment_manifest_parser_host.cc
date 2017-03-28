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

namespace payments {

PaymentManifestParserHost::PaymentManifestParserHost() {}

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

void PaymentManifestParserHost::ParsePaymentManifest(const std::string& content,
                                                     Callback callback) {
  if (!mojo_client_) {
    std::move(callback).Run(std::vector<mojom::PaymentManifestSectionPtr>());
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
  DCHECK_GE(10U, pending_callbacks_.size());
  Callback* callback_identifier = &pending_callbacks_.back();

  mojo_client_->service()->Parse(
      content, base::Bind(&PaymentManifestParserHost::OnParse,
                          base::Unretained(this), callback_identifier));
}

void PaymentManifestParserHost::OnParse(
    const Callback* callback_identifier,
    std::vector<mojom::PaymentManifestSectionPtr> manifest) {
  // At most 10 manifests to parse, so iterating a vector is not too slow.
  const auto& pending_callback_it =
      std::find_if(pending_callbacks_.begin(), pending_callbacks_.end(),
                   [callback_identifier](const Callback& pending_callback) {
                     return &pending_callback == callback_identifier;
                   });
  if (pending_callback_it == pending_callbacks_.end()) {
    // If unable to find the pending callback, then something went wrong in the
    // utility process. Stop the utility process and notify all callbacks.
    OnUtilityProcessStopped();
    return;
  }

  Callback callback = std::move(*pending_callback_it);
  pending_callbacks_.erase(pending_callback_it);

  // Can trigger synchronous deletion of this object, so can't access any of
  // the member variables after this block.
  std::move(callback).Run(std::move(manifest));
}

void PaymentManifestParserHost::OnUtilityProcessStopped() {
  mojo_client_.reset();
  std::vector<Callback> callbacks = std::move(pending_callbacks_);
  for (Callback& callback : callbacks) {
    // Can trigger synchronous deletion of this object, so can't access any of
    // the member variables after this line.
    std::move(callback).Run(std::vector<mojom::PaymentManifestSectionPtr>());
  }
}

}  // namespace payments
