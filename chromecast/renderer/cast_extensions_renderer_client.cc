// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_extensions_renderer_client.h"

#include "base/memory/ptr_util.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/dispatcher_delegate.h"

namespace extensions {

CastExtensionsRendererClient::CastExtensionsRendererClient()
    : dispatcher_(std::make_unique<Dispatcher>(
          std::make_unique<DispatcherDelegate>())) {}

CastExtensionsRendererClient::~CastExtensionsRendererClient() {}

bool CastExtensionsRendererClient::IsIncognitoProcess() const {
  // cast_shell doesn't support off-the-record contexts.
  return false;
}

int CastExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // cast_shell doesn't need to reserve world IDs for anything other than
  // extensions, so we always return 1. Note that 0 is reserved for the global
  // world.
  return 1;
}

Dispatcher* CastExtensionsRendererClient::GetDispatcher() {
  return dispatcher_.get();
}

}  // namespace extensions
