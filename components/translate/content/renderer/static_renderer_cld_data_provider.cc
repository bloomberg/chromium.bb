// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "static_renderer_cld_data_provider.h"

#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_message.h"

namespace translate {
// Implementation of the static factory method from RendererCldDataProvider,
// hooking up this specific implementation for all of Chromium.
RendererCldDataProvider* CreateRendererCldDataProviderFor(
    content::RenderViewObserver* render_view_observer) {
  return new StaticRendererCldDataProvider();
}

StaticRendererCldDataProvider::StaticRendererCldDataProvider() {
}

StaticRendererCldDataProvider::~StaticRendererCldDataProvider() {
}

bool StaticRendererCldDataProvider::OnMessageReceived(
    const IPC::Message& message) {
  // No-op: data is statically linked
  return false;
}

void StaticRendererCldDataProvider::SendCldDataRequest() {
  // No-op: data is statically linked
}

bool StaticRendererCldDataProvider::IsCldDataAvailable() {
  // No-op: data is statically linked
  return true;
}

void StaticRendererCldDataProvider::SetCldAvailableCallback(
    base::Callback<void(void)> callback) {
  // Data is statically linked, so just call immediately.
  callback.Run();
}

}  // namespace translate
