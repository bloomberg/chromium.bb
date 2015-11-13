// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_application_delegate.h"

#include "mojo/application/public/cpp/application_connection.h"

namespace mojo {
namespace shell {

ShellApplicationDelegate::ShellApplicationDelegate(
    mojo::shell::ApplicationManager* manager)
    : manager_(manager) {}
ShellApplicationDelegate::~ShellApplicationDelegate() {}

void ShellApplicationDelegate::Initialize(ApplicationImpl* app) {}
bool ShellApplicationDelegate::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<mojom::ApplicationManager>(this);
  return true;
}

void ShellApplicationDelegate::Create(
    ApplicationConnection* connection,
    InterfaceRequest<mojom::ApplicationManager> request) {
  bindings_.AddBinding(this, request.Pass());
}

void ShellApplicationDelegate::CreateInstanceForHandle(ScopedHandle channel) {
  // TODO(beng): create the instance.
}

}  // namespace shell
}  // namespace mojo
