// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/sms/sms_service_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace content {

// static
std::unique_ptr<SmsService> SmsService::Create() {
  return std::make_unique<SmsServiceImpl>();
}

SmsServiceImpl::SmsServiceImpl() {}

SmsServiceImpl::~SmsServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SmsServiceImpl::CreateService(blink::mojom::SmsManagerRequest request,
                                   const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void SmsServiceImpl::GetNextMessage(base::TimeDelta timeout,
                                    GetNextMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timeout <= base::TimeDelta::FromSeconds(0)) {
    bindings_.ReportBadMessage("Invalid timeout.");
    return;
  }

  if (!sms_provider_)
    sms_provider_ = SmsProvider::Create();

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

void SmsServiceImpl::SetSmsProviderForTest(
    std::unique_ptr<SmsProvider> sms_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sms_provider_ = std::move(sms_provider);
}

}  // namespace content
