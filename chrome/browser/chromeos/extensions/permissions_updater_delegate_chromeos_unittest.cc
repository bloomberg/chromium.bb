// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/permissions_updater_delegate_chromeos.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/login/login_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
const char kBogusId[] = "bogus";

// TODO(isandrk, crbug.com/715638): Extract MockManifestPermission into its own
// file (since it's duplicated in two places).
class MockManifestPermission : public ManifestPermission {
 public:
  MockManifestPermission(const std::string& name)
      : name_(name) {
  }

  std::string name() const override { return name_; }

  std::string id() const override { return name(); }

  PermissionIDSet GetPermissions() const override { return PermissionIDSet(); }

  bool FromValue(const base::Value* value) override { return true; }

  std::unique_ptr<base::Value> ToValue() const override {
    return base::MakeUnique<base::Value>();
  }

  ManifestPermission* Diff(const ManifestPermission* rhs) const override {
    const MockManifestPermission* other =
        static_cast<const MockManifestPermission*>(rhs);
    EXPECT_EQ(name_, other->name_);
    return NULL;
  }

  ManifestPermission* Union(const ManifestPermission* rhs) const override {
    const MockManifestPermission* other =
        static_cast<const MockManifestPermission*>(rhs);
    EXPECT_EQ(name_, other->name_);
    return new MockManifestPermission(name_);
  }

  ManifestPermission* Intersect(const ManifestPermission* rhs) const override {
    const MockManifestPermission* other =
        static_cast<const MockManifestPermission*>(rhs);
    EXPECT_EQ(name_, other->name_);
    return new MockManifestPermission(name_);
  }

 private:
  std::string name_;
};

scoped_refptr<Extension> CreateExtension(const std::string& id) {
  std::string error;
  base::DictionaryValue manifest;
  manifest.SetString(manifest_keys::kName, "test");
  manifest.SetString(manifest_keys::kVersion, "0.1");
  scoped_refptr<Extension> extension = Extension::Create(
      base::FilePath(),
      Manifest::INTERNAL,
      manifest,
      Extension::NO_FLAGS,
      id,
      &error);
  return extension;
}

std::unique_ptr<const PermissionSet> CreatePermissions(
    bool include_clipboard = true) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kAudio);
  apis.insert(APIPermission::kFullscreen);
  if (include_clipboard)
    apis.insert(APIPermission::kClipboardRead);
  ManifestPermissionSet manifest;
  manifest.insert(new MockManifestPermission("author"));
  manifest.insert(new MockManifestPermission("background"));
  URLPatternSet explicit_hosts({
      URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/*"),
      URLPattern(URLPattern::SCHEME_ALL, "<all_urls>")});
  URLPatternSet scriptable_hosts({
    URLPattern(URLPattern::SCHEME_ALL, "http://www.wikipedia.com/*")});
  auto permissions = base::MakeUnique<const PermissionSet>(
      apis, manifest, explicit_hosts, scriptable_hosts);
  return permissions;
}

}  // namespace

TEST(PermissionsUpdaterDelegateChromeOSTest, NoFilteringOutsidePublicSession) {
  PermissionsUpdaterDelegateChromeOS delegate;
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  // Whitelisted extension outside PS, nothing filtered.
  auto extension = CreateExtension(kWhitelistedId);
  auto granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);

  // Bogus extension ID (never whitelisted) outside PS, nothing filtered.
  extension = CreateExtension(kBogusId);
  granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);
}

TEST(PermissionsUpdaterDelegateChromeOSTest,
     FilterNonWhitelistedInsidePublicSession) {
  PermissionsUpdaterDelegateChromeOS delegate;
  // Set Public Session state.
  chromeos::LoginState::Initialize();
  chromeos::LoginState::Get()->SetLoggedInState(
    chromeos::LoginState::LOGGED_IN_ACTIVE,
    chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  // Whitelisted extension, nothing gets filtered.
  auto extension = CreateExtension(kWhitelistedId);
  auto granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(), *granted_permissions);

  // Bogus extension ID (never whitelisted), ClipboardRead filtered out,
  // everything else stays.
  extension = CreateExtension(kBogusId);
  granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_EQ(*CreatePermissions(false), *granted_permissions);

  // Reset state at the end of test.
  chromeos::LoginState::Shutdown();
}

}  // namespace extensions
