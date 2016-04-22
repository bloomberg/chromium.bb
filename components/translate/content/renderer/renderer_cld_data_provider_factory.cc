// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOT DEAD CODE!
// This code isn't dead, even if it isn't currently being used. Please refer to:
// https://www.chromium.org/developers/how-tos/compact-language-detector-cld-data-source-configuration

#include "components/translate/content/renderer/renderer_cld_data_provider_factory.h"

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"
#include "content/public/renderer/render_frame_observer.h"

namespace {

// The global instance, alive for the entire lifetime of the process.
translate::RendererCldDataProviderFactory* g_instance = NULL;

// The default factory, which produces no-op instances of
// RendererCldDataProvider suitable for use when CLD data is statically-linked.
base::LazyInstance<translate::RendererCldDataProviderFactory>::Leaky
  g_wrapped_default = LAZY_INSTANCE_INITIALIZER;

}  // namespace


namespace translate {

std::unique_ptr<RendererCldDataProvider>
RendererCldDataProviderFactory::CreateRendererCldDataProvider(
    content::RenderFrameObserver* render_frame_observer) {
  return base::WrapUnique(new RendererCldDataProvider());
}

// static
bool RendererCldDataProviderFactory::IsInitialized() {
  return g_instance != nullptr;
}

// static
void RendererCldDataProviderFactory::SetDefault(
    RendererCldDataProviderFactory* instance) {
  if (!IsInitialized()) Set(instance);
}

// static
void RendererCldDataProviderFactory::Set(
    RendererCldDataProviderFactory* instance) {
    g_instance = instance;
}

// static
RendererCldDataProviderFactory* RendererCldDataProviderFactory::Get() {
  if (IsInitialized()) return g_instance;
  return &g_wrapped_default.Get();
}

}  // namespace translate
