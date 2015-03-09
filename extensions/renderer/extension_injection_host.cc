// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/renderer/extension_injection_host.h"

namespace extensions {

ExtensionInjectionHost::ExtensionInjectionHost(
    const Extension* extension)
    : InjectionHost(HostID(HostID::EXTENSIONS, extension->id())),
      extension_(extension) {
}

ExtensionInjectionHost::~ExtensionInjectionHost() {
}

// static
scoped_ptr<const InjectionHost> ExtensionInjectionHost::Create(
    const std::string& extension_id,
    const ExtensionSet* extensions) {
  const Extension* extension = extensions->GetByID(extension_id);
  if (!extension)
    return scoped_ptr<const ExtensionInjectionHost>();
  return scoped_ptr<const ExtensionInjectionHost>(
      new ExtensionInjectionHost(extension));
}

std::string ExtensionInjectionHost::GetContentSecurityPolicy() const {
  return CSPInfo::GetContentSecurityPolicy(extension_);
}

const GURL& ExtensionInjectionHost::url() const {
  return extension_->url();
}

const std::string& ExtensionInjectionHost::name() const {
  return extension_->name();
}

PermissionsData::AccessType ExtensionInjectionHost::CanExecuteOnFrame(
    const GURL& document_url,
    const GURL& top_frame_url,
    int tab_id,
    bool is_declarative) const {
  // If we don't have a tab id, we have no UI surface to ask for user consent.
  // For now, we treat this as an automatic allow.
  if (tab_id == -1)
    return PermissionsData::ACCESS_ALLOWED;

  // Declarative user scripts use "page access" (from "permissions" section in
  // manifest) whereas non-declarative user scripts use custom
  // "content script access" logic.
  if (is_declarative) {
    return extension_->permissions_data()->GetPageAccess(
        extension_,
        document_url,
        top_frame_url,
        tab_id,
        -1,  // no process id
        nullptr /* ignore error */);
  } else {
    return extension_->permissions_data()->GetContentScriptAccess(
        extension_,
        document_url,
        top_frame_url,
        tab_id,
        -1,  // no process id
        nullptr /* ignore error */);
  }
}

bool ExtensionInjectionHost::ShouldNotifyBrowserOfInjection() const {
  // We notify the browser of any injection if the extension has no withheld
  // permissions (i.e., the permissions weren't restricted), but would have
  // otherwise been affected by the scripts-require-action feature.
  return extension_->permissions_data()->withheld_permissions()->IsEmpty() &&
         PermissionsData::ScriptsMayRequireActionForExtension(
             extension_,
             extension_->permissions_data()->active_permissions().get());
}

}  // namespace extensions
