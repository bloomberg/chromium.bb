// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_app_connection_impl.h"

#include "content/browser/mojo/mojo_shell_context.h"

namespace content {

scoped_ptr<MojoAppConnection> MojoAppConnection::Create(const GURL& url) {
  return scoped_ptr<MojoAppConnection>(new MojoAppConnectionImpl(url));
}

MojoAppConnectionImpl::MojoAppConnectionImpl(const GURL& url) {
  MojoShellContext::ConnectToApplication(url, mojo::GetProxy(&services_));
}

MojoAppConnectionImpl::~MojoAppConnectionImpl() {
}

void MojoAppConnectionImpl::ConnectToService(
    const std::string& service_name,
    mojo::ScopedMessagePipeHandle handle) {
  services_->ConnectToService(service_name, handle.Pass());
}

}  // namespace content
