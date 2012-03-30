// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_nacl.h"

#include "base/file_util.h"
#include "base/logging.h"

// This file is currently a stub to get us linking.
// TODO(brettw) implement this.

namespace IPC {

ChannelImpl::ChannelImpl(const IPC::ChannelHandle& channel_handle,
                         Mode mode,
                         Listener* listener)
    : ChannelReader(listener) {
}

ChannelImpl::~ChannelImpl() {
  Close();
}

bool ChannelImpl::Connect() {
  NOTIMPLEMENTED();
  return false;
}

void ChannelImpl::Close() {
  NOTIMPLEMENTED();
}

bool ChannelImpl::Send(Message* message) {
  NOTIMPLEMENTED();
}

int ChannelImpl::GetClientFileDescriptor() const {
  NOTIMPLEMENTED();
  return -1;
}

int ChannelImpl::TakeClientFileDescriptor() {
  NOTIMPLEMENTED();
  return -1;
}

bool ChannelImpl::AcceptsConnections() const {
  NOTIMPLEMENTED();
  return false;
}

bool ChannelImpl::HasAcceptedConnection() const {
  NOTIMPLEMENTED();
  return false;
}

bool ChannelImpl::GetClientEuid(uid_t* client_euid) const {
  NOTIMPLEMENTED();
  return false;
}

void ChannelImpl::ResetToAcceptingConnectionState() {
  NOTIMPLEMENTED();
}

// static
bool Channel::ChannelImpl::IsNamedServerInitialized(
    const std::string& channel_id) {
  return file_util::PathExists(FilePath(channel_id));
}

//------------------------------------------------------------------------------
// Channel's methods simply call through to ChannelImpl.

Channel::Channel(const IPC::ChannelHandle& channel_handle,
                 Mode mode,
                 Listener* listener)
    : channel_impl_(new ChannelImpl(channel_handle, mode, listener)) {
}

Channel::~Channel() {
  delete channel_impl_;
}

bool Channel::Connect() {
  return channel_impl_->Connect();
}

void Channel::Close() {
  channel_impl_->Close();
}

void Channel::set_listener(Listener* listener) {
  channel_impl_->set_listener(listener);
}

bool Channel::Send(Message* message) {
  return channel_impl_->Send(message);
}

int Channel::GetClientFileDescriptor() const {
  return channel_impl_->GetClientFileDescriptor();
}

int Channel::TakeClientFileDescriptor() {
  return channel_impl_->TakeClientFileDescriptor();
}

bool Channel::AcceptsConnections() const {
  return channel_impl_->AcceptsConnections();
}

bool Channel::HasAcceptedConnection() const {
  return channel_impl_->HasAcceptedConnection();
}

bool Channel::GetClientEuid(uid_t* client_euid) const {
  return channel_impl_->GetClientEuid(client_euid);
}

void Channel::ResetToAcceptingConnectionState() {
  channel_impl_->ResetToAcceptingConnectionState();
}

// static
bool Channel::IsNamedServerInitialized(const std::string& channel_id) {
  return ChannelImpl::IsNamedServerInitialized(channel_id);
}

// static
std::string Channel::GenerateVerifiedChannelID(const std::string& prefix) {
  // A random name is sufficient validation on posix systems, so we don't need
  // an additional shared secret.
  std::string id = prefix;
  if (!id.empty())
    id.append(".");

  return id.append(GenerateUniqueRandomChannelID());
}

}  // namespace IPC
