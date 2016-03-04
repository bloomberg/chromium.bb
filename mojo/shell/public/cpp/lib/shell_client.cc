// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {

ShellClient::ShellClient() {}
ShellClient::~ShellClient() {}

void ShellClient::Initialize(Connector* connector, const std::string& name,
                             uint32_t id, uint32_t user_id) {
}

bool ShellClient::AcceptConnection(Connection* connection) {
  return false;
}

void ShellClient::ShellConnectionLost() {}

}  // namespace mojo
