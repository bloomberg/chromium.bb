// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_authenticator.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebCredential.h"

namespace test_runner {

namespace {

std::vector<uint8_t> ArrayBufferViewToUint8Vector(
    const gin::ArrayBufferView& view) {
  std::vector<uint8_t> data;
  uint8_t* bytes = static_cast<uint8_t*>(view.bytes());
  data.resize(view.num_bytes());
  std::copy(bytes, bytes + view.num_bytes(), data.begin());
  return data;
}

}  // namespace

MockAuthenticator::MockAuthenticator()
    : error_(webauth::mojom::AuthenticatorStatus::NOT_IMPLEMENTED) {
  ClearResponse();
}

MockAuthenticator::~MockAuthenticator() {}

void MockAuthenticator::BindRequest(
    webauth::mojom::AuthenticatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

// Tests should pass in 0-length arrays to zero out attestation_object,
// authenticator_data, or signature, as needed.
void MockAuthenticator::SetResponse(
    const std::string& id,
    const gin::ArrayBufferView& raw_id,
    const gin::ArrayBufferView& client_data_json,
    const gin::ArrayBufferView& attestation_object,
    const gin::ArrayBufferView& authenticator_data,
    const gin::ArrayBufferView& signature) {
  public_key_credential_->id = id;
  public_key_credential_->raw_id = ArrayBufferViewToUint8Vector(raw_id);
  public_key_credential_->client_data_json =
      ArrayBufferViewToUint8Vector(client_data_json);
  webauth::mojom::AuthenticatorResponsePtr response =
      webauth::mojom::AuthenticatorResponse::New();
  if (attestation_object.num_bytes()) {
    response->attestation_object =
        ArrayBufferViewToUint8Vector(attestation_object);
  }

  if (authenticator_data.num_bytes()) {
    response->authenticator_data =
        ArrayBufferViewToUint8Vector(authenticator_data);
  }

  if (signature.num_bytes()) {
    response->signature = ArrayBufferViewToUint8Vector(signature);
  }
  public_key_credential_->response = std::move(response);
}

void MockAuthenticator::ClearResponse() {
  public_key_credential_ = webauth::mojom::PublicKeyCredentialInfo::New();
}

void MockAuthenticator::SetError(const std::string& error) {
  if (error == "cancelled")
    error_ = webauth::mojom::AuthenticatorStatus::CANCELLED;
  if (error == "unknown")
    error_ = webauth::mojom::AuthenticatorStatus::UNKNOWN_ERROR;
  if (error == "not allowed")
    error_ = webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  if (error == "not supported")
    error_ = webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR;
  if (error == "security")
    error_ = webauth::mojom::AuthenticatorStatus::SECURITY_ERROR;
  if (error == "not implemented")
    error_ = webauth::mojom::AuthenticatorStatus::NOT_IMPLEMENTED;
  if (error.empty())
    error_ = webauth::mojom::AuthenticatorStatus::SUCCESS;
}

void MockAuthenticator::MakeCredential(
    webauth::mojom::MakeCredentialOptionsPtr options,
    MakeCredentialCallback callback) {
  if (error_ != webauth::mojom::AuthenticatorStatus::SUCCESS) {
    std::move(callback).Run(error_, nullptr);
  } else {
    std::move(callback).Run(error_, std::move(public_key_credential_));
    ClearResponse();
  }
}
}  // namespace test_runner