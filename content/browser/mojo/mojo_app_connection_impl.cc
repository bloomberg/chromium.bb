// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_app_connection_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "mojo/shell/capability_filter.h"

namespace content {

const char kBrowserMojoAppUrl[] = "system:content_browser";

namespace {
void OnGotRemoteIDs(uint32_t remote_id, uint32_t content_handler_id) {}
}  // namespace

// static
scoped_ptr<MojoAppConnection> MojoAppConnection::Create(
    const GURL& url,
    const GURL& requestor_url) {
  return scoped_ptr<MojoAppConnection>(
      new MojoAppConnectionImpl(url, requestor_url));
}

MojoAppConnectionImpl::MojoAppConnectionImpl(const GURL& url,
                                             const GURL& requestor_url) {
  MojoShellContext::ConnectToApplication(
      url, requestor_url, mojo::GetProxy(&interfaces_),
      mojo::shell::mojom::InterfaceProviderPtr(),
      mojo::shell::GetPermissiveCapabilityFilter(),
      base::Bind(&OnGotRemoteIDs));
}

MojoAppConnectionImpl::~MojoAppConnectionImpl() {
}

void MojoAppConnectionImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  interfaces_->GetInterface(interface_name, std::move(handle));
}

}  // namespace content
