// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_fetcher.h"

#include "extensions/browser/extension_registry.h"

namespace extensions {

ContentHashFetcher::ContentHashFetcher(content::BrowserContext* context,
                                       ContentVerifierDelegate* delegate)
    : context_(context), delegate_(delegate), observer_(this) {
}

ContentHashFetcher::~ContentHashFetcher() {
}

void ContentHashFetcher::Start() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  observer_.Add(registry);
}

void ContentHashFetcher::DoFetch(const Extension* extension) {
}

void ContentHashFetcher::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
}

void ContentHashFetcher::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
}

}  // namespace extensions
