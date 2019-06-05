// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include "base/strings/string_piece.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

namespace content {
namespace protocol {

namespace {
static constexpr char kAuthenticatorNotFound[] =
    "Could not find a Virtual Authenticator matching the ID";
static constexpr char kInvalidProtocol[] = "The protocol is not valid";
static constexpr char kInvalidTransport[] = "The transport is not valid";
static constexpr char kVirtualEnvironmentNotEnabled[] =
    "The Virtual Authenticator Environment has not been enabled for this "
    "session";

device::ProtocolVersion ConvertToProtocolVersion(base::StringPiece protocol) {
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::Ctap2)
    return device::ProtocolVersion::kCtap2;
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::U2f)
    return device::ProtocolVersion::kU2f;
  return device::ProtocolVersion::kUnknown;
}

}  // namespace

WebAuthnHandler::WebAuthnHandler()
    : DevToolsDomainHandler(WebAuthn::Metainfo::domainName) {}

WebAuthnHandler::~WebAuthnHandler() = default;

void WebAuthnHandler::SetRenderer(int process_host_id,
                                  RenderFrameHostImpl* frame_host) {
  frame_host_ = frame_host;
}

void WebAuthnHandler::Wire(UberDispatcher* dispatcher) {
  WebAuthn::Dispatcher::wire(dispatcher, this);
}

Response WebAuthnHandler::Enable() {
  AuthenticatorEnvironmentImpl::GetInstance()->EnableVirtualAuthenticatorFor(
      frame_host_->frame_tree_node());
  virtual_discovery_factory_ =
      AuthenticatorEnvironmentImpl::GetInstance()->GetVirtualFactoryFor(
          frame_host_->frame_tree_node());
  return Response::OK();
}

Response WebAuthnHandler::Disable() {
  AuthenticatorEnvironmentImpl::GetInstance()->DisableVirtualAuthenticatorFor(
      frame_host_->frame_tree_node());
  virtual_discovery_factory_ = nullptr;
  return Response::OK();
}

Response WebAuthnHandler::AddVirtualAuthenticator(
    std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
    String* out_authenticator_id) {
  if (!virtual_discovery_factory_)
    return Response::Error(kVirtualEnvironmentNotEnabled);

  auto transport =
      device::ConvertToFidoTransportProtocol(options->GetTransport());
  if (!transport)
    return Response::InvalidParams(kInvalidTransport);

  auto protocol = ConvertToProtocolVersion(options->GetProtocol());
  if (protocol == device::ProtocolVersion::kUnknown)
    return Response::InvalidParams(kInvalidProtocol);

  auto* authenticator = virtual_discovery_factory_->CreateAuthenticator(
      protocol, *transport,
      transport == device::FidoTransportProtocol::kInternal
          ? device::AuthenticatorAttachment::kPlatform
          : device::AuthenticatorAttachment::kCrossPlatform,
      options->GetHasResidentKey(), options->GetHasUserVerification());

  *out_authenticator_id = authenticator->unique_id();
  return Response::OK();
}

Response WebAuthnHandler::RemoveVirtualAuthenticator(
    const String& authenticator_id) {
  if (!virtual_discovery_factory_)
    return Response::Error(kVirtualEnvironmentNotEnabled);

  if (!virtual_discovery_factory_->RemoveAuthenticator(authenticator_id))
    return Response::Error(kAuthenticatorNotFound);

  return Response::OK();
}

}  // namespace protocol
}  // namespace content
