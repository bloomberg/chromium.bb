// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/shell/service.h"

namespace mojo {

ServiceFactoryBase::ServiceFactoryBase(ScopedMessagePipeHandle shell_handle)
    : shell_(shell_handle.Pass(), this) {
}

ServiceFactoryBase::~ServiceFactoryBase() {}

}  // namespace mojo
