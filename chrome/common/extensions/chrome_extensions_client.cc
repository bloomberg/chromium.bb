// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/features/base_feature_provider.h"

namespace extensions {

static base::LazyInstance<ChromeExtensionsClient> g_client =
    LAZY_INSTANCE_INITIALIZER;

ChromeExtensionsClient::ChromeExtensionsClient()
    :  chrome_api_permissions_(ChromeAPIPermissions()) {
}

ChromeExtensionsClient::~ChromeExtensionsClient() {
}

const PermissionsProvider&
ChromeExtensionsClient::GetPermissionsProvider() const {
  return chrome_api_permissions_;
}

FeatureProvider* ChromeExtensionsClient::GetFeatureProviderByName(
    const std::string& name) const {
  return BaseFeatureProvider::GetByName(name);
}

void ChromeExtensionsClient::RegisterManifestHandlers() const {
  RegisterChromeManifestHandlers();
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
