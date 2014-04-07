// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/common/shell_extensions_client.h"

#include "apps/shell/browser/shell_app_runtime_api.h"
#include "base/logging.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/api/sockets/sockets_manifest_handler.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/manifest_handler.h"
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
  extensions::RegisterCommonManifestHandlers();

  // TODO(rockot): API manifest handlers which move out to src/extensions
  // should either end up in RegisterCommonManifestHandlers or some new
  // initialization step specifically for API manifest handlers.
  (new extensions::SocketsManifestHandler)->Register();

  extensions::ManifestHandler::FinalizeRegistration();
  // TODO(jamescook): Do we need to whitelist any extensions?
}

const extensions::PermissionsProvider&
ShellExtensionsClient::GetPermissionsProvider() const {
  // TODO(jamescook): app_shell needs a way to use a subset of the Chrome
  // extension Features and Permissions. In particular, the lists of Features
  // (including API features, manifest features and permission features) are
  // listed in JSON files from c/c/e/api that are included into Chrome's
  // resources.pak (_api_features.json and _permission_features.json). The
  // PermissionsProvider must match the set of permissions used by the features
  // in those files.  We either need to make app_shell (and hence the extensions
  // module) know about all possible permissions, or create a mechanism whereby
  // we can build our own JSON files with only a subset of the data. For now,
  // just provide all permissions Chrome knows about. Fixing this issue is
  // http://crbug.com/339301
  static extensions::ChromeAPIPermissions provider;
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
  // TODO(jamescook): Factor out an extensions module feature provider.
  return extensions::BaseFeatureProvider::GetByName(name);
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

bool ShellExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  // TODO(rockot): Remove dependency on src/chrome once we have some core APIs
  // moved out. See http://crbug.com/349042.
  // Special-case our simplified app.runtime implementation because we don't
  // have the Chrome app APIs available.
  return extensions::api::GeneratedSchemas::IsGenerated(name) ||
         extensions::core_api::GeneratedSchemas::IsGenerated(name) ||
         name == extensions::ShellAppRuntimeAPI::GetName();
}

base::StringPiece ShellExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // TODO(rockot): Remove dependency on src/chrome once we have some core APIs
  // moved out. See http://crbug.com/349042.
  if (extensions::api::GeneratedSchemas::IsGenerated(name))
    return extensions::api::GeneratedSchemas::Get(name);

  // Special-case our simplified app.runtime implementation.
  if (name == extensions::ShellAppRuntimeAPI::GetName())
    return extensions::ShellAppRuntimeAPI::GetSchema();

  return extensions::core_api::GeneratedSchemas::Get(name);
}

void ShellExtensionsClient::AddExtraFeatureFilters(
    extensions::SimpleFeature* feature) const {}

}  // namespace apps
