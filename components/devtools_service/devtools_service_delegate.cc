// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_service_delegate.h"

#include "base/logging.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace devtools_service {

DevToolsServiceDelegate::DevToolsServiceDelegate() {}

DevToolsServiceDelegate::~DevToolsServiceDelegate() {}

void DevToolsServiceDelegate::Initialize(mojo::ApplicationImpl* app) {
  NOTIMPLEMENTED();
}

bool DevToolsServiceDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  NOTIMPLEMENTED();
  return false;
}

void DevToolsServiceDelegate::Quit() {
  NOTIMPLEMENTED();
}

}  // namespace devtools_service
