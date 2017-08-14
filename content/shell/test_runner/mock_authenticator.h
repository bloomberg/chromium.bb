// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_AUTHENTICATOR_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_AUTHENTICATOR_H_

#include <memory>

#include "base/macros.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "gin/array_buffer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/webauth/authenticator.mojom.h"

namespace test_runner {

class TEST_RUNNER_EXPORT MockAuthenticator
    : public NON_EXPORTED_BASE(webauth::mojom::Authenticator) {
 public:
  MockAuthenticator();
  ~MockAuthenticator() override;

  void BindRequest(webauth::mojom::AuthenticatorRequest request);

  void SetResponse(const std::string& id,
                   const gin::ArrayBufferView& raw_id,
                   const gin::ArrayBufferView& client_data_json,
                   const gin::ArrayBufferView& attestation_object,
                   const gin::ArrayBufferView& authenticator_data,
                   const gin::ArrayBufferView& signature);
  void ClearResponse();
  void SetError(const std::string& error);

  // webauth::mojom::Authenticator:
  void MakeCredential(webauth::mojom::MakeCredentialOptionsPtr options,
                      MakeCredentialCallback callback) override;

 private:
  std::vector<uint8_t> ConvertArrayBufferView(const gin::ArrayBufferView& view);

  mojo::BindingSet<webauth::mojom::Authenticator> bindings_;

  webauth::mojom::PublicKeyCredentialInfoPtr public_key_credential_;
  webauth::mojom::AuthenticatorStatus error_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_AUTHENTICATOR_H_