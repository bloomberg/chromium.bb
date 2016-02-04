// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_application_delegate.h"

#include <stdint.h>

#include <utility>

#include "base/process/process.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

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
  bindings_.AddBinding(this, std::move(request));
}

void ShellApplicationDelegate::CreateInstanceForHandle(
    ScopedHandle channel,
    const String& url,
    CapabilityFilterPtr filter,
    InterfaceRequest<mojom::PIDReceiver> pid_receiver) {
  manager_->CreateInstanceForHandle(std::move(channel), GURL(url.get()),
                                    std::move(filter), std::move(pid_receiver));
}

void ShellApplicationDelegate::AddListener(
    mojom::ApplicationManagerListenerPtr listener) {
  manager_->AddListener(std::move(listener));
}

}  // namespace shell
}  // namespace mojo
