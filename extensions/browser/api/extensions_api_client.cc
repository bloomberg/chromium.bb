// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {
namespace {

ExtensionsAPIClient* g_instance = NULL;

}  // namespace

ExtensionsAPIClient::ExtensionsAPIClient() { g_instance = this; }

ExtensionsAPIClient::~ExtensionsAPIClient() { g_instance = NULL; }

// static
ExtensionsAPIClient* ExtensionsAPIClient::Get() { return g_instance; }

void ExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >& observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {}

}  // namespace extensions
