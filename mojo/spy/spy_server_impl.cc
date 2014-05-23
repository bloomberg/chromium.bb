// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/spy/spy_server_impl.h"

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/system/core.h"

namespace {

bool NextId(uint32_t* out_id) {
  static uint32_t id = 1;
  if (!++id)
    return false;
  *out_id = id;
  return true;
}

}  // namespace

namespace mojo {

struct SpyServerImpl::Item {
  enum Type {
    kServiceIntercept,
    kMessage
  };

  uint32_t id;
  Type type;

  Item(uint32_t id, Type type) : id(id), type(type) {}
};

SpyServerImpl::SpyServerImpl() : has_session_(false) {
  BindToPipe(this, pipe_.handle0.Pass());
}

SpyServerImpl::~SpyServerImpl() {
}

void SpyServerImpl::StartSession(
    const spy_api::Version& version,
    const mojo::Callback<void(spy_api::Result, mojo::String)>& callback) {
  AllocationScope scope;
  if (has_session_) {
    callback.Run(spy_api::RESOURCE_LIMIT, "");
    return;
  }
  callback.Run(spy_api::ALL_OK, "session 0");
  has_session_ = true;
}

void SpyServerImpl::StopSession(
  const mojo::Callback<void(spy_api::Result)>& callback) {
  AllocationScope scope;
  if (!has_session_) {
    callback.Run(spy_api::INVALID_CALL);
    return;
  }
  callback.Run(spy_api::ALL_OK);
  has_session_ = false;
}

void SpyServerImpl::TrackConnection(
  uint32_t id,
  spy_api::ConnectionOptions options,
  const mojo::Callback<void(spy_api::Result)>& callback) {
}

void SpyServerImpl::OnConnectionError() {
  // Pipe got disconnected.
}

void SpyServerImpl::OnIntercept(const GURL& url) {
  if (!has_session_)
    return;

  AllocationScope scope;
  uint32_t id;
  if (!NextId(&id)) {
    client()->OnFatalError(spy_api::NO_MORE_IDS);
    return;
  }

  items_[id] = new Item(id, Item::kServiceIntercept);
  client()->OnClientConnection(
      url.possibly_invalid_spec(), id, spy_api::PEEK_MESSAGES);
}

ScopedMessagePipeHandle SpyServerImpl::ServerPipe() {
  return ScopedMessagePipeHandle(pipe_.handle1.Pass()).Pass();
}

}  // namespace mojo
