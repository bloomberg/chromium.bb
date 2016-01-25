// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"

#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"

namespace extensions {

namespace errors = manifest_errors;

RendererPermissionsPolicyDelegate::RendererPermissionsPolicyDelegate(
    Dispatcher* dispatcher) : dispatcher_(dispatcher) {
  PermissionsData::SetPolicyDelegate(this);
}
RendererPermissionsPolicyDelegate::~RendererPermissionsPolicyDelegate() {
  PermissionsData::SetPolicyDelegate(NULL);
}

bool RendererPermissionsPolicyDelegate::CanExecuteScriptOnPage(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) {
  if (PermissionsData::CanExecuteScriptEverywhere(extension))
    return true;

  if (dispatcher_->IsExtensionActive(kWebStoreAppId)) {
    if (error)
      *error = errors::kCannotScriptGallery;
    return false;
  }

  return true;
}

}  // namespace extensions
