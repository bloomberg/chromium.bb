// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_app_connection_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "content/browser/mojo/mojo_shell_context.h"

namespace content {

const char kBrowserMojoAppUrl[] = "system:content_browser";

namespace {
void OnGotInstanceID(mojo::shell::mojom::ConnectResult result,
                     const std::string& user_id, uint32_t remote_id) {}
}  // namespace

// static
scoped_ptr<MojoAppConnection> MojoAppConnection::Create(
    const std::string& name,
    const std::string& requestor_name) {
  return scoped_ptr<MojoAppConnection>(
      new MojoAppConnectionImpl(name, requestor_name));
}

MojoAppConnectionImpl::MojoAppConnectionImpl(
    const std::string& name,
    const std::string& requestor_name) {
  MojoShellContext::ConnectToApplication(
      name, requestor_name, mojo::GetProxy(&interfaces_),
      mojo::shell::mojom::InterfaceProviderPtr(),
      base::Bind(&OnGotInstanceID));
}

MojoAppConnectionImpl::~MojoAppConnectionImpl() {
}

void MojoAppConnectionImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  interfaces_->GetInterface(interface_name, std::move(handle));
}

}  // namespace content
