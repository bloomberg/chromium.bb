// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/single_client_message_proxy_impl.h"

#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
SingleClientMessageProxyImpl::Factory*
    SingleClientMessageProxyImpl::Factory::test_factory_ = nullptr;

// static
SingleClientMessageProxyImpl::Factory*
SingleClientMessageProxyImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SingleClientMessageProxyImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  test_factory_ = factory;
}

SingleClientMessageProxyImpl::Factory::~Factory() = default;

std::unique_ptr<SingleClientMessageProxy>
SingleClientMessageProxyImpl::Factory::BuildInstance(
    SingleClientMessageProxy::Delegate* delegate,
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  return base::WrapUnique(new SingleClientMessageProxyImpl(
      delegate, feature, std::move(connection_delegate_ptr)));
}

SingleClientMessageProxyImpl::SingleClientMessageProxyImpl(
    SingleClientMessageProxy::Delegate* delegate,
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr)
    : SingleClientMessageProxy(delegate),
      feature_(feature),
      connection_delegate_ptr_(std::move(connection_delegate_ptr)),
      channel_(std::make_unique<ChannelImpl>(this /* delegate */)) {
  DCHECK(connection_delegate_ptr_);
  connection_delegate_ptr_->OnConnection(
      channel_->GenerateInterfacePtr(),
      mojo::MakeRequest(&message_receiver_ptr_));
}

SingleClientMessageProxyImpl::~SingleClientMessageProxyImpl() = default;

void SingleClientMessageProxyImpl::HandleReceivedMessage(
    const std::string& feature,
    const std::string& payload) {
  // Ignore messages intended for other clients.
  if (feature != feature_)
    return;

  message_receiver_ptr_->OnMessageReceived(payload);
}

void SingleClientMessageProxyImpl::HandleRemoteDeviceDisconnection() {
  channel_->HandleRemoteDeviceDisconnection();
}

void SingleClientMessageProxyImpl::OnSendMessageRequested(
    const std::string& message,
    base::OnceClosure on_sent_callback) {
  NotifySendMessageRequested(feature_, message, std::move(on_sent_callback));
}

const mojom::ConnectionMetadata&
SingleClientMessageProxyImpl::GetConnectionMetadata() {
  return GetConnectionMetadataFromDelegate();
}

void SingleClientMessageProxyImpl::OnClientDisconnected() {
  NotifyClientDisconnected();
}

void SingleClientMessageProxyImpl::FlushForTesting() {
  DCHECK(connection_delegate_ptr_);
  connection_delegate_ptr_.FlushForTesting();

  DCHECK(message_receiver_ptr_);
  message_receiver_ptr_.FlushForTesting();
}

}  // namespace secure_channel

}  // namespace chromeos
