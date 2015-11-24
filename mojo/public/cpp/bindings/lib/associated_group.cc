// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_group.h"

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

namespace mojo {

AssociatedGroup::AssociatedGroup() {}

AssociatedGroup::AssociatedGroup(const AssociatedGroup& other)
    : router_(other.router_) {}

AssociatedGroup::~AssociatedGroup() {}

AssociatedGroup& AssociatedGroup::operator=(const AssociatedGroup& other) {
  if (this == &other)
    return *this;

  router_ = other.router_;
  return *this;
}

void AssociatedGroup::CreateEndpointHandlePair(
    internal::ScopedInterfaceEndpointHandle* local_endpoint,
    internal::ScopedInterfaceEndpointHandle* remote_endpoint) {
  if (!router_)
    return;

  router_->CreateEndpointHandlePair(local_endpoint, remote_endpoint);
}

}  // namespace mojo
