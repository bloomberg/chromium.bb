// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/renderer/extensions/dispatcher.h"

namespace extensions {

namespace errors = extension_manifest_errors;

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
    const UserScript* script,
    int process_id,
    std::string* error) {
  const Extension::ScriptingWhitelist* whitelist =
      Extension::GetScriptingWhitelist();
  if (std::find(whitelist->begin(), whitelist->end(), extension->id()) !=
      whitelist->end()) {
    return true;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSigninProcess)) {
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
