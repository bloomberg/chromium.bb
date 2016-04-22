// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_extensions_client.h"

#include "base/stl_util.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/extensions_api_permissions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/test_permission_message_provider.h"
#include "grit/extensions_resources.h"

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

}  // namespace

TestExtensionsClient::TestExtensionsClient() {
}

TestExtensionsClient::~TestExtensionsClient() {
}

void TestExtensionsClient::AddBrowserImagePathsFilter(
    BrowserImagePathsFilter* filter) {
  browser_image_filters_.insert(filter);
}

void TestExtensionsClient::RemoveBrowserImagePathsFilter(
    BrowserImagePathsFilter* filter) {
  browser_image_filters_.erase(filter);
}

void TestExtensionsClient::Initialize() {
  // Registration could already be finalized in unit tests, where the utility
  // thread runs in-process.
  if (!ManifestHandler::IsRegistrationFinalized()) {
    RegisterCommonManifestHandlers();
    ManifestHandler::FinalizeRegistration();
  }

  // Allow the core API permissions.
  static ExtensionsAPIPermissions extensions_api_permissions;
  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions);
}

const PermissionMessageProvider&
TestExtensionsClient::GetPermissionMessageProvider() const {
  static TestPermissionMessageProvider provider;
  return provider;
}

const std::string TestExtensionsClient::GetProductName() {
  return "extensions_test";
}

std::unique_ptr<FeatureProvider> TestExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  std::unique_ptr<FeatureProvider> provider;
  std::unique_ptr<JSONFeatureProviderSource> source(
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
  } else if (name == "behavior") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<BehaviorFeature>));
  } else {
    NOTREACHED();
  }
  return provider;
}

std::unique_ptr<JSONFeatureProviderSource>
TestExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  std::unique_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource(name));
  if (name == "api") {
    source->LoadJSON(IDR_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_EXTENSION_PERMISSION_FEATURES);
  } else if (name == "behavior") {
    source->LoadJSON(IDR_EXTENSION_BEHAVIOR_FEATURES);
  } else {
    NOTREACHED();
    source.reset();
  }
  return source;
}

void TestExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
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
  return api::GeneratedSchemas::IsGenerated(name);
}

base::StringPiece TestExtensionsClient::GetAPISchema(
    const std::string& name) const {
  return api::GeneratedSchemas::Get(name);
}

void TestExtensionsClient::RegisterAPISchemaResources(ExtensionAPI* api) const {
}

bool TestExtensionsClient::ShouldSuppressFatalErrors() const {
  return true;
}

void TestExtensionsClient::RecordDidSuppressFatalError() {
}

std::string TestExtensionsClient::GetWebstoreBaseURL() const {
  return extension_urls::kChromeWebstoreBaseURL;
}

std::string TestExtensionsClient::GetWebstoreUpdateURL() const {
  return extension_urls::kChromeWebstoreUpdateURL;
}

bool TestExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  return true;
}

std::set<base::FilePath> TestExtensionsClient::GetBrowserImagePaths(
    const Extension* extension) {
  std::set<base::FilePath> result =
      ExtensionsClient::GetBrowserImagePaths(extension);
  for (auto filter : browser_image_filters_)
    filter->Filter(extension, &result);
  return result;
}

}  // namespace extensions
