// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_extensions_client.h"

#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/test/test_permission_message_provider.h"
#include "extensions/test/test_permissions_provider.h"

namespace extensions {

TestExtensionsClient::TestExtensionsClient() {
}

TestExtensionsClient::~TestExtensionsClient() {
}

void TestExtensionsClient::Initialize() {
  // Registration could already be finalized in unit tests, where the utility
  // thread runs in-process.
  if (!ManifestHandler::IsRegistrationFinalized()) {
    RegisterCommonManifestHandlers();
    ManifestHandler::FinalizeRegistration();
  }
}

const PermissionsProvider&
TestExtensionsClient::GetPermissionsProvider() const {
  static TestPermissionsProvider provider;
  return provider;
}

const PermissionMessageProvider&
TestExtensionsClient::GetPermissionMessageProvider() const {
  static TestPermissionMessageProvider provider;
  return provider;
}

FeatureProvider* TestExtensionsClient::GetFeatureProviderByName(
    const std::string& name) const {
  return BaseFeatureProvider::GetByName(name);
}

void TestExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    std::set<PermissionMessage>* messages) const {
}

void TestExtensionsClient::SetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const ExtensionsClient::ScriptingWhitelist&
TestExtensionsClient::GetScriptingWhitelist() const {
  return scripting_whitelist_;
}

URLPatternSet TestExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  URLPatternSet hosts;
  return hosts;
}

bool TestExtensionsClient::IsScriptableURL(const GURL& url,
                                           std::string* error) const {
  return true;
}

bool TestExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  return false;
}

base::StringPiece TestExtensionsClient::GetAPISchema(
    const std::string& name) const {
  return base::StringPiece();
}

void TestExtensionsClient::AddExtraFeatureFilters(
    SimpleFeature* feature) const {
}

bool TestExtensionsClient::ShouldSuppressFatalErrors() const {
  return true;
}

}  // namespace extensions
