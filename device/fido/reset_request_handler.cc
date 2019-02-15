// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "device/fido/reset_request_handler.h"

namespace device {

ResetRequestHandler::ResetRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    ResetSentCallback reset_sent_callback,
    FinishedCallback finished_callback)
    : FidoRequestHandlerBase(connector, supported_transports),
      reset_sent_callback_(std::move(reset_sent_callback)),
      finished_callback_(std::move(finished_callback)),
      weak_factory_(this) {
  Start();
}

ResetRequestHandler::~ResetRequestHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  CancelActiveAuthenticators();
}

void ResetRequestHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  authenticator->GetTouch(base::BindOnce(&ResetRequestHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void ResetRequestHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (processed_touch_) {
    return;
  }

  processed_touch_ = true;
  CancelActiveAuthenticators();

  if (authenticator->SupportedProtocol() != ProtocolVersion::kCtap) {
    std::move(finished_callback_)
        .Run(CtapDeviceResponseCode::kCtap1ErrInvalidCommand);
    return;
  }

  authenticator->Reset(base::BindOnce(&ResetRequestHandler::OnResetComplete,
                                      weak_factory_.GetWeakPtr()));
  std::move(reset_sent_callback_).Run();
}

void ResetRequestHandler::OnResetComplete(
    CtapDeviceResponseCode status,
    base::Optional<pin::EmptyResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(processed_touch_);

  if (status == CtapDeviceResponseCode::kSuccess && !response) {
    status = CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  std::move(finished_callback_).Run(status);
}

}  // namespace device
