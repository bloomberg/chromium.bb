// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

constexpr char kMakeCredentialType[] = "navigator.id.makeCredential";

// JSON key values
constexpr char kTypeKey[] = "type";
constexpr char kChallengeKey[] = "challenge";
constexpr char kOriginKey[] = "origin";
constexpr char kCidPubkeyKey[] = "cid_pubkey";

// Serializes the |value| to a JSON string and returns the result.
std::string SerializeValueToJson(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

}  // namespace

// static
void AuthenticatorImpl::Create(
    RenderFrameHost* render_frame_host,
    webauth::mojom::AuthenticatorRequest request) {
  auto authenticator_impl =
      base::WrapUnique(new AuthenticatorImpl(render_frame_host));
  mojo::MakeStrongBinding(std::move(authenticator_impl), std::move(request));
}

AuthenticatorImpl::~AuthenticatorImpl() {}

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  caller_origin_ = render_frame_host->GetLastCommittedOrigin();
}

// mojom:Authenticator
void AuthenticatorImpl::MakeCredential(
    webauth::mojom::MakeCredentialOptionsPtr options,
    MakeCredentialCallback callback) {
  std::string effective_domain;
  std::string relying_party_id;
  std::string client_data_json;
  base::DictionaryValue client_data;

  // Steps 6 & 7 of https://w3c.github.io/webauthn/#createCredential
  // opaque origin
  if (caller_origin_.unique()) {
    std::move(callback).Run(
        webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
    return;
  }

  if (!options->relying_party->id) {
    relying_party_id = caller_origin_.Serialize();
  } else {
    effective_domain = caller_origin_.host();

    DCHECK(!effective_domain.empty());
    // TODO(kpaulhamus): Check if relyingPartyId is a registrable domain
    // suffix of and equal to effectiveDomain and set relyingPartyId
    // appropriately.
    relying_party_id = *options->relying_party->id;
  }

  // Check that at least one of the cryptographic parameters is supported.
  // Only ES256 is currently supported by U2F_V2.
  if (!HasValidAlgorithm(options->crypto_parameters)) {
    std::move(callback).Run(
        webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR, nullptr);
    return;
  }

  client_data.SetString(kTypeKey, kMakeCredentialType);
  client_data.SetString(kChallengeKey,
                        base::StringPiece(reinterpret_cast<const char*>(
                                              options->challenge.data()),
                                          options->challenge.size()));
  client_data.SetString(kOriginKey, relying_party_id);
  // Channel ID is optional, and missing if the browser doesn't support it.
  // It is present and set to the constant "unused" if the browser
  // supports Channel ID but is not using it to talk to the origin.
  // TODO(kpaulhamus): Fetch and add the Channel ID public key used to
  // communicate with the origin.
  client_data.SetString(kCidPubkeyKey, "unused");

  // SHA-256 hash the JSON data structure
  client_data_json = SerializeValueToJson(client_data);
  std::string client_data_hash = crypto::SHA256HashString(client_data_json);

  std::move(callback).Run(webauth::mojom::AuthenticatorStatus::NOT_IMPLEMENTED,
                          nullptr);
}

bool AuthenticatorImpl::HasValidAlgorithm(
    const std::vector<webauth::mojom::PublicKeyCredentialParametersPtr>&
        parameters) {
  for (const auto& params : parameters) {
    if (params->algorithm_identifier == -7)
      return true;
  }
  return false;
}
}  // namespace content
