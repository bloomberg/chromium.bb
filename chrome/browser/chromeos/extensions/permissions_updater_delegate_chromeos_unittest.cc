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
#include "extensions/common/permissions/permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
const char kBogusId[] = "bogus";

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

std::unique_ptr<const PermissionSet> CreatePermissions() {
  APIPermissionSet apis;
  apis.insert(APIPermission::kAudio);
  apis.insert(APIPermission::kClipboardRead);
  apis.insert(APIPermission::kFullscreen);
  auto permissions = base::MakeUnique<const PermissionSet>(
      apis, ManifestPermissionSet(),
      URLPatternSet(), URLPatternSet());
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

  // Bogus extension ID (never whitelisted), ClipboardRead filtered out.
  extension = CreateExtension(kBogusId);
  granted_permissions = CreatePermissions();
  delegate.InitializePermissions(extension.get(), &granted_permissions);
  EXPECT_FALSE(granted_permissions->HasAPIPermission(
                   APIPermission::kClipboardRead));
  EXPECT_EQ(2u, granted_permissions->apis().size());

  // Reset state at the end of test.
  chromeos::LoginState::Shutdown();
}

}  // namespace extensions
