// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/sms/sms_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"

namespace content {

SmsManager::SmsManager() : sms_provider_(SmsProvider::Create()) {}

SmsManager::~SmsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SmsManager::CreateService(blink::mojom::SmsManagerRequest request,
                               const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void SmsManager::GetNextMessage(base::TimeDelta timeout,
                                GetNextMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timeout <= base::TimeDelta::FromSeconds(0)) {
    bindings_.ReportBadMessage("Invalid timeout.");
    return;
  }

  sms_provider_->Retrieve(
      timeout, base::BindOnce(
                   [](GetNextMessageCallback callback, bool success,
                      base::Optional<std::string> sms) {
                     if (!success) {
                       std::move(callback).Run(blink::mojom::SmsMessage::New(
                           blink::mojom::SmsStatus::kTimeout, base::nullopt));
                       return;
                     }
                     std::move(callback).Run(blink::mojom::SmsMessage::New(
                         blink::mojom::SmsStatus::kSuccess, std::move(sms)));
                   },
                   std::move(callback)));
}

void SmsManager::SetSmsProviderForTest(
    std::unique_ptr<SmsProvider> sms_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sms_provider_ = std::move(sms_provider);
}

}  // namespace content
