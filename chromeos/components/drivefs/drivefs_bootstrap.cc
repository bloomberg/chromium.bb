// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_bootstrap.h"

#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

namespace drivefs {

DriveFsBootstrapListener::DriveFsBootstrapListener()
    : bootstrap_(mojo::MakeProxy(mojom::DriveFsBootstrapPtrInfo(
          invitation_.AttachMessagePipe("drivefs-bootstrap"),
          mojom::DriveFsBootstrap::Version_))),
      pending_token_(base::UnguessableToken::Create()) {
  PendingConnectionManager::Get().ExpectOpenIpcChannel(
      pending_token_,
      base::BindOnce(&DriveFsBootstrapListener::AcceptMojoConnection,
                     base::Unretained(this)));
}

DriveFsBootstrapListener::~DriveFsBootstrapListener() {
  if (pending_token_) {
    PendingConnectionManager::Get().CancelExpectedOpenIpcChannel(
        pending_token_);
    pending_token_ = {};
  }
}

mojom::DriveFsBootstrapPtr DriveFsBootstrapListener::bootstrap() {
  return std::move(bootstrap_);
}

void DriveFsBootstrapListener::AcceptMojoConnection(base::ScopedFD handle) {
  DCHECK(pending_token_);
  pending_token_ = {};
  connected_ = true;
  SendInvitationOverPipe(std::move(handle));
}

void DriveFsBootstrapListener::SendInvitationOverPipe(base::ScopedFD handle) {
  mojo::OutgoingInvitation::Send(
      std::move(invitation_), base::kNullProcessHandle,
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(handle))));
}

DriveFsConnection::DriveFsConnection(
    std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener,
    mojom::DriveFsConfigurationPtr config,
    mojom::DriveFsDelegate* delegate,
    base::OnceClosure on_disconnected)
    : bootstrap_listener_(std::move(bootstrap_listener)),
      delegate_binding_(delegate),
      on_disconnected_(std::move(on_disconnected)) {
  auto bootstrap = bootstrap_listener_->bootstrap();

  mojom::DriveFsDelegatePtr delegate_ptr;
  delegate_binding_.Bind(mojo::MakeRequest(&delegate_ptr));
  delegate_binding_.set_connection_error_handler(base::BindOnce(
      &DriveFsConnection::OnMojoConnectionError, base::Unretained(this)));

  bootstrap->Init(std::move(config), mojo::MakeRequest(&drivefs_),
                  std::move(delegate_ptr));

  drivefs_.set_connection_error_handler(base::BindOnce(
      &DriveFsConnection::OnMojoConnectionError, base::Unretained(this)));
}

DriveFsConnection::~DriveFsConnection() {
  CleanUp();
}

void DriveFsConnection::CleanUp() {
  if (on_disconnected_ && bootstrap_listener_->is_connected()) {
    std::move(on_disconnected_).Run();
  }
}

void DriveFsConnection::OnMojoConnectionError() {
  CleanUp();
}

}  // namespace drivefs
