// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

// static
void KeepAliveImpl::Create(content::BrowserContext* context,
                           const Extension* extension,
                           mojo::InterfaceRequest<KeepAlive> request) {
  mojo::BindToRequest(new KeepAliveImpl(context, extension), &request);
}

KeepAliveImpl::KeepAliveImpl(content::BrowserContext* context,
                             const Extension* extension)
    : context_(context), extension_(extension) {
  ExtensionSystem* system = ExtensionSystem::Get(context_);
  if (!system)
    return;
  system->process_manager()->IncrementLazyKeepaliveCount(extension_);
}

KeepAliveImpl::~KeepAliveImpl() {
  ExtensionSystem* system = ExtensionSystem::Get(context_);
  if (!system)
    return;
  system->process_manager()->DecrementLazyKeepaliveCount(extension_);
}

}  // namespace extensions
