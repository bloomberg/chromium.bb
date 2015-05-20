// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include "extensions/browser/process_manager.h"

namespace extensions {

// static
void KeepAliveImpl::Create(content::BrowserContext* context,
                           const Extension* extension,
                           mojo::InterfaceRequest<KeepAlive> request) {
  new KeepAliveImpl(context, extension, request.Pass());
}

KeepAliveImpl::KeepAliveImpl(content::BrowserContext* context,
                             const Extension* extension,
                             mojo::InterfaceRequest<KeepAlive> request)
    : context_(context), extension_(extension), binding_(this, request.Pass()) {
  ProcessManager::Get(context_)->IncrementLazyKeepaliveCount(extension_);
}

KeepAliveImpl::~KeepAliveImpl() {
  ProcessManager::Get(context_)->DecrementLazyKeepaliveCount(extension_);
}

}  // namespace extensions
