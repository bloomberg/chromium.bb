// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

namespace mojo {
namespace internal {

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle()
    : ScopedInterfaceEndpointHandle(kInvalidInterfaceId, true, nullptr) {}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    InterfaceId id,
    bool is_local,
    scoped_refptr<MultiplexRouter> router)
    : id_(id), is_local_(is_local), router_(std::move(router)) {
  DCHECK(!IsValidInterfaceId(id) || router_);
}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    ScopedInterfaceEndpointHandle&& other)
    : id_(other.id_), is_local_(other.is_local_) {
  router_.swap(other.router_);
  other.id_ = kInvalidInterfaceId;
}

ScopedInterfaceEndpointHandle::~ScopedInterfaceEndpointHandle() {
  reset();
}

ScopedInterfaceEndpointHandle& ScopedInterfaceEndpointHandle::operator=(
    ScopedInterfaceEndpointHandle&& other) {
  reset();
  swap(other);

  return *this;
}

void ScopedInterfaceEndpointHandle::reset() {
  if (!IsValidInterfaceId(id_))
    return;

  router_->CloseEndpointHandle(id_, is_local_);

  id_ = kInvalidInterfaceId;
  is_local_ = true;
  router_ = nullptr;
}

void ScopedInterfaceEndpointHandle::swap(ScopedInterfaceEndpointHandle& other) {
  using std::swap;
  swap(other.id_, id_);
  swap(other.is_local_, is_local_);
  swap(other.router_, router_);
}

InterfaceId ScopedInterfaceEndpointHandle::release() {
  InterfaceId result = id_;

  id_ = kInvalidInterfaceId;
  is_local_ = true;
  router_ = nullptr;

  return result;
}

}  // namespace internal
}  // namespace mojo
