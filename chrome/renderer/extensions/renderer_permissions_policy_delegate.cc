// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
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
    const GURL& top_document_url,
    int tab_id,
    int process_id,
    std::string* error) {
  const ExtensionsClient::ScriptingWhitelist& whitelist =
      ExtensionsClient::Get()->GetScriptingWhitelist();
  if (std::find(whitelist.begin(), whitelist.end(), extension->id()) !=
      whitelist.end()) {
    return true;
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kSigninProcess)) {
    if (error)
      *error = errors::kCannotScriptSigninPage;
    return false;
  }

  if (dispatcher_->IsExtensionActive(extension_misc::kWebStoreAppId)) {
    if (error)
      *error = errors::kCannotScriptGallery;
    return false;
  }

  return true;
}

}  // namespace extensions
