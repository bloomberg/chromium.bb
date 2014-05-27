// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/service_connector.h"

namespace mojo {
namespace internal {

ServiceConnectorBase::Owner::Owner(
    ScopedMessagePipeHandle service_provider_handle) {
  service_provider_.Bind(service_provider_handle.Pass());
  service_provider_.set_client(this);
}

ServiceConnectorBase::Owner::~Owner() {}

ServiceConnectorBase::~ServiceConnectorBase() {}

}  // namespace internal
}  // namespace mojo
