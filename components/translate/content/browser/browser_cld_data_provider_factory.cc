// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOT DEAD CODE!
// This code isn't dead, even if it isn't currently being used. Please refer to:
// https://www.chromium.org/developers/how-tos/compact-language-detector-cld-data-source-configuration

#include "components/translate/content/browser/browser_cld_data_provider_factory.h"

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"

namespace {

// The global instance, alive for the entire lifetime of the process.
translate::BrowserCldDataProviderFactory* g_instance = NULL;

// The default factory, which produces no-op instances of BrowserCldDataProvider
// suitable for use when CLD data is statically-linked.
base::LazyInstance<translate::BrowserCldDataProviderFactory>::Leaky
  g_wrapped_default = LAZY_INSTANCE_INITIALIZER;

}  // namespace


namespace translate {

std::unique_ptr<BrowserCldDataProvider>
BrowserCldDataProviderFactory::CreateBrowserCldDataProvider(
    content::WebContents* web_contents) {
  return base::WrapUnique(new BrowserCldDataProvider);
}

// static
bool BrowserCldDataProviderFactory::IsInitialized() {
  return g_instance != NULL;
}

// static
void BrowserCldDataProviderFactory::SetDefault(
    BrowserCldDataProviderFactory* instance) {
  if (!IsInitialized()) Set(instance);
}

// static
void BrowserCldDataProviderFactory::Set(
    BrowserCldDataProviderFactory* instance) {
    g_instance = instance;
}

// static
BrowserCldDataProviderFactory* BrowserCldDataProviderFactory::Get() {
  if (IsInitialized()) return g_instance;
  return &g_wrapped_default.Get();
}

}  // namespace translate
