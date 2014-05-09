// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/shell/service.h"

namespace mojo {
namespace internal {

ServiceConnectorBase::Owner::Owner(ScopedMessagePipeHandle shell_handle)
    : shell_(MakeProxy<Shell>(shell_handle.Pass())) {
  shell_->SetClient(this);
}

ServiceConnectorBase::Owner::~Owner() {}

ServiceConnectorBase::~ServiceConnectorBase() {}

}  // namespace internal
}  // namespace mojo
