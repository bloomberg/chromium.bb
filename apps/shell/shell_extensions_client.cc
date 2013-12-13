// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_extensions_client.h"

#include "base/logging.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_provider.h"
#include "extensions/common/url_pattern_set.h"

using extensions::APIPermissionInfo;
using extensions::APIPermissionSet;
using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;
using extensions::URLPatternSet;

namespace apps {

namespace {

// TODO(jamescook): Refactor ChromeAPIPermissions to share some of the
// permissions registration for app_shell. For now, allow no permissions.
class ShellPermissionsProvider : public extensions::PermissionsProvider {
 public:
  ShellPermissionsProvider() {}
  virtual ~ShellPermissionsProvider() {}

  virtual std::vector<APIPermissionInfo*> GetAllPermissions() const OVERRIDE {
    return std::vector<APIPermissionInfo*>();
  }

  virtual std::vector<AliasInfo> GetAllAliases() const OVERRIDE {
    return std::vector<AliasInfo>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellPermissionsProvider);
};

// TODO(jamescook): Refactor ChromePermissionsMessageProvider so we can share
// code.
class ShellPermissionMessageProvider
    : public extensions::PermissionMessageProvider {
 public:
  ShellPermissionMessageProvider() {}
  virtual ~ShellPermissionMessageProvider() {}

  // PermissionMessageProvider implementation.
  virtual PermissionMessages GetPermissionMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE {
    return PermissionMessages();
  }

  virtual std::vector<base::string16> GetWarningMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE {
    return std::vector<base::string16>();
  }

  virtual std::vector<base::string16> GetWarningMessagesDetails(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE {
    return std::vector<base::string16>();
  }

  virtual bool IsPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions,
      Manifest::Type extension_type) const OVERRIDE {
    // Ensure we implement this before shipping.
    CHECK(false);
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellPermissionMessageProvider);
};

}  // namespace

ShellExtensionsClient::ShellExtensionsClient() {
}

ShellExtensionsClient::~ShellExtensionsClient() {
}

void ShellExtensionsClient::Initialize() {
  // TODO(jamescook): Do we need to whitelist any extensions?
}

const extensions::PermissionsProvider&
ShellExtensionsClient::GetPermissionsProvider() const {
  NOTIMPLEMENTED();
  static ShellPermissionsProvider provider;
  return provider;
}

const extensions::PermissionMessageProvider&
ShellExtensionsClient::GetPermissionMessageProvider() const {
  NOTIMPLEMENTED();
  static ShellPermissionMessageProvider provider;
  return provider;
}

extensions::FeatureProvider* ShellExtensionsClient::GetFeatureProviderByName(
    const std::string& name) const {
  NOTIMPLEMENTED();
  return NULL;
}

void ShellExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    std::set<PermissionMessage>* messages) const {
  NOTIMPLEMENTED();
}

void ShellExtensionsClient::SetScriptingWhitelist(
    const ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const extensions::ExtensionsClient::ScriptingWhitelist&
ShellExtensionsClient::GetScriptingWhitelist() const {
  // TODO(jamescook): Real whitelist.
  return scripting_whitelist_;
}

URLPatternSet ShellExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  NOTIMPLEMENTED();
  return URLPatternSet();
}

bool ShellExtensionsClient::IsScriptableURL(const GURL& url,
                                            std::string* error) const {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace apps
