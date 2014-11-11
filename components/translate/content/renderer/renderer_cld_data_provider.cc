// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"

namespace {

// Our global default instance, alive for the entire lifetime of the process.
translate::RendererCldDataProvider* g_instance = NULL;

// The default factory, which produces no-op instances of
// RendererCldDataProvider suitable for use when CLD data is statically-linked.
base::LazyInstance<translate::RendererCldDataProvider>::Leaky
g_wrapped_default = LAZY_INSTANCE_INITIALIZER;

}  // namespace


namespace translate {

bool RendererCldDataProvider::IsCldDataAvailable() {
  return true;
}

bool RendererCldDataProvider::OnMessageReceived(const IPC::Message& message) {
  return false;  // Message not handled
}

void RendererCldDataProvider::SetCldAvailableCallback
(base::Callback<void(void)> callback) {
  callback.Run();
}

// static
void RendererCldDataProvider::SetDefault(
    RendererCldDataProvider* instance) {
  if (!IsInitialized()) Set(instance);
}

// static
void RendererCldDataProvider::Set(
    RendererCldDataProvider* instance) {
  g_instance = instance;
}

// static
bool RendererCldDataProvider::IsInitialized() {
  return g_instance != NULL;
}

// static
RendererCldDataProvider* RendererCldDataProvider::Get() {
  if (IsInitialized()) return g_instance;
  return &g_wrapped_default.Get();
}

}  // namespace translate
