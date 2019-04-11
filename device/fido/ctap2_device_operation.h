// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
#define DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/device_operation.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

// Represents per device logic for CTAP2 authenticators. Ctap2DeviceOperation
// is owned by FidoTask, and thus |request| outlives Ctap2DeviceOperation.
template <class Request, class Response>
class Ctap2DeviceOperation : public DeviceOperation<Request, Response> {
 public:
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<Response>)>;
  using DeviceResponseParser =
      base::OnceCallback<base::Optional<Response>(base::span<const uint8_t>)>;

  Ctap2DeviceOperation(FidoDevice* device,
                       Request request,
                       DeviceResponseCallback callback,
                       DeviceResponseParser device_response_parser)
      : DeviceOperation<Request, Response>(device,
                                           std::move(request),
                                           std::move(callback)),
        device_response_parser_(std::move(device_response_parser)),
        weak_factory_(this) {}

  ~Ctap2DeviceOperation() override = default;

  void Start() override {
    std::pair<CtapRequestCommand, base::Optional<cbor::Value>> request(
        this->request().EncodeAsCBOR());
    std::vector<uint8_t> request_bytes;

    // TODO: it would be nice to see which device each request is going to, but
    // that breaks every mock test because they aren't expecting a call to
    // GetId().
    if (request.second) {
      FIDO_LOG(DEBUG) << "<- " << static_cast<int>(request.first) << " "
                      << cbor::DiagnosticWriter::Write(*request.second);
      base::Optional<std::vector<uint8_t>> cbor_bytes =
          cbor::Writer::Write(*request.second);
      DCHECK(cbor_bytes);
      request_bytes = std::move(*cbor_bytes);
    } else {
      FIDO_LOG(DEBUG) << "<- " << static_cast<int>(request.first)
                      << " (no payload)";
    }

    request_bytes.insert(request_bytes.begin(),
                         static_cast<uint8_t>(request.first));

    this->token_ = this->device()->DeviceTransact(
        std::move(request_bytes),
        base::BindOnce(&Ctap2DeviceOperation::OnResponseReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnResponseReceived(
      base::Optional<std::vector<uint8_t>> device_response) {
    this->token_.reset();

    if (!device_response) {
      std::move(this->callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      return;
    }

    const auto response_code = GetResponseCode(*device_response);
    std::move(this->callback())
        .Run(response_code, std::move(device_response_parser_)
                                .Run(std::move(*device_response)));
  }

 private:
  DeviceResponseParser device_response_parser_;
  base::WeakPtrFactory<Ctap2DeviceOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ctap2DeviceOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
