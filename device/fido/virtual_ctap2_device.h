// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/virtual_fido_device.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualCtap2Device
    : public VirtualFidoDevice {
 public:
  struct COMPONENT_EXPORT(DEVICE_FIDO) Config {
    Config();

    bool pin_support = false;
    bool internal_uv_support = false;
  };

  VirtualCtap2Device();
  explicit VirtualCtap2Device(scoped_refptr<State> state, const Config& config);
  ~VirtualCtap2Device() override;

  // FidoDevice:
  void Cancel() override;
  void DeviceTransact(std::vector<uint8_t> command, DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  void SetAuthenticatorSupportedOptions(
      const AuthenticatorSupportedOptions& options);

 private:
  CtapDeviceResponseCode OnMakeCredential(base::span<const uint8_t> request,
                                          std::vector<uint8_t>* response);

  CtapDeviceResponseCode OnGetAssertion(base::span<const uint8_t> request,
                                        std::vector<uint8_t>* response);

  CtapDeviceResponseCode OnPINCommand(base::span<const uint8_t> request,
                                      std::vector<uint8_t>* response);

  CtapDeviceResponseCode OnAuthenticatorGetInfo(
      std::vector<uint8_t>* response) const;

  AuthenticatorData ConstructAuthenticatorData(
      base::span<const uint8_t, kRpIdHashLength> rp_id_hash,
      bool user_verified,
      uint32_t current_signature_count,
      base::Optional<AttestedCredentialData> attested_credential_data,
      base::Optional<cbor::Value> extensions);

  base::WeakPtrFactory<FidoDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VirtualCtap2Device);
};

// Decodes a CBOR-encoded CTAP2 authenticatorMakeCredential request message. The
// request's client_data_json() value will be empty, and the hashed client data
// is returned separately.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::pair<CtapMakeCredentialRequest,
                         CtapMakeCredentialRequest::ClientDataHash>>
ParseCtapMakeCredentialRequest(base::span<const uint8_t> request_bytes);

// Decodes a CBOR-encoded CTAP2 authenticatorGetAssertion request message. The
// request's client_data_json() value will be empty, and the hashed client data
// is returned separately.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<
    std::pair<CtapGetAssertionRequest, CtapGetAssertionRequest::ClientDataHash>>
ParseCtapGetAssertionRequest(base::span<const uint8_t> request_bytes);

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
