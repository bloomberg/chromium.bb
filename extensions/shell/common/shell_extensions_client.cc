// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_extensions_client.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/api/sockets/sockets_manifest_handler.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/permissions_provider.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/shell/common/api/generated_schemas.h"
#include "grit/app_shell_resources.h"
#include "grit/extensions_resources.h"

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

// TODO(jamescook): Refactor ChromePermissionsMessageProvider so we can share
// code. For now, this implementation does nothing.
class ShellPermissionMessageProvider : public PermissionMessageProvider {
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

base::LazyInstance<ShellPermissionMessageProvider>
    g_permission_message_provider = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShellExtensionsClient::ShellExtensionsClient()
    : extensions_api_permissions_(ExtensionsAPIPermissions()) {
}

ShellExtensionsClient::~ShellExtensionsClient() {
}

void ShellExtensionsClient::Initialize() {
  RegisterCommonManifestHandlers();

  // TODO(rockot): API manifest handlers which move out to src/extensions
  // should either end up in RegisterCommonManifestHandlers or some new
  // initialization step specifically for API manifest handlers.
  (new SocketsManifestHandler)->Register();

  ManifestHandler::FinalizeRegistration();
  // TODO(jamescook): Do we need to whitelist any extensions?

  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_);
}

const PermissionMessageProvider&
ShellExtensionsClient::GetPermissionMessageProvider() const {
  NOTIMPLEMENTED();
  return g_permission_message_provider.Get();
}

scoped_ptr<FeatureProvider> ShellExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  scoped_ptr<FeatureProvider> provider;
  scoped_ptr<JSONFeatureProviderSource> source(
      CreateFeatureProviderSource(name));
  if (name == "api") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<APIFeature>));
  } else if (name == "manifest") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<ManifestFeature>));
  } else if (name == "permission") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<PermissionFeature>));
  } else {
    NOTREACHED();
  }
  return provider.Pass();
}

scoped_ptr<JSONFeatureProviderSource>
ShellExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  scoped_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource(name));
  if (name == "api") {
    source->LoadJSON(IDR_EXTENSION_API_FEATURES);
    source->LoadJSON(IDR_SHELL_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_EXTENSION_PERMISSION_FEATURES);
  } else {
    NOTREACHED();
    source.reset();
  }
  return source.Pass();
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

const ExtensionsClient::ScriptingWhitelist&
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
  return core_api::GeneratedSchemas::IsGenerated(name) ||
         shell_api::GeneratedSchemas::IsGenerated(name);
}

base::StringPiece ShellExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // Schema for chrome.shell APIs.
  if (shell_api::GeneratedSchemas::IsGenerated(name))
    return shell_api::GeneratedSchemas::Get(name);

  return core_api::GeneratedSchemas::Get(name);
}

void ShellExtensionsClient::RegisterAPISchemaResources(
    ExtensionAPI* api) const {
}

bool ShellExtensionsClient::ShouldSuppressFatalErrors() const {
  return true;
}

}  // namespace extensions
