// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_test_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile) {
  scoped_ptr<DictionaryValue> manifest(new DictionaryValue);
  manifest->SetString(extension_manifest_keys::kName, name);
  manifest->SetString(extension_manifest_keys::kVersion, "0.1");
  manifest->SetInteger(extension_manifest_keys::kManifestVersion, 2);
  ListValue* background_script_list = new ListValue;
  background_script_list->Append(Value::CreateStringValue("background.js"));
  manifest->Set(extension_manifest_keys::kPlatformAppBackgroundScripts,
                background_script_list);

  ListValue* permission_detail_list = new ListValue;
  for (size_t i = 0; i < media_galleries_permissions.size(); i++)
    permission_detail_list->Append(
        Value::CreateStringValue(media_galleries_permissions[i]));
  DictionaryValue* media_galleries_permission = new DictionaryValue();
  media_galleries_permission->Set("mediaGalleries", permission_detail_list);
  ListValue* permission_list = new ListValue;
  permission_list->Append(media_galleries_permission);
  manifest->Set(extension_manifest_keys::kPermissions, permission_list);


  base::FilePath path = profile->GetPath().AppendASCII(name);
  std::string errors;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(path, extensions::Manifest::INTERNAL,
                                    *manifest.get(),
                                    extensions::Extension::NO_FLAGS, &errors);
  EXPECT_TRUE(extension.get() != NULL) << errors;
  EXPECT_TRUE(extensions::Extension::IdIsValid(extension->id()));
  if (!extension.get() || !extensions::Extension::IdIsValid(extension->id()))
    return NULL;

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->extension_prefs()->OnExtensionInstalled(
      extension.get(), extensions::Extension::ENABLED,
      syncer::StringOrdinal::CreateInitialOrdinal());
  extension_service->AddExtension(extension);
  extension_service->EnableExtension(extension->id());

  return extension;
}

EnsureMediaDirectoriesExists::EnsureMediaDirectoriesExists()
    : num_galleries_(0) {
  Init();
}

void EnsureMediaDirectoriesExists::Init() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  return;
#else
  ASSERT_TRUE(fake_dir_.CreateUniqueTempDir());

  const int kDirectoryKeys[] = {
    chrome::DIR_USER_MUSIC,
    chrome::DIR_USER_PICTURES,
    chrome::DIR_USER_VIDEOS,
  };

  const char* kDirectoryNames[] = {
    "music",
    "pictures",
    "videos",
  };

  for (size_t i = 0; i < arraysize(kDirectoryKeys); ++i) {
    PathService::OverrideAndCreateIfNeeded(
        kDirectoryKeys[i], fake_dir_.path().AppendASCII(kDirectoryNames[i]),
        true /*create*/);
    base::FilePath path;
    if (PathService::Get(kDirectoryKeys[i], &path) &&
        file_util::DirectoryExists(path)) {
      ++num_galleries_;
    }
  }
  ASSERT_GT(num_galleries_, 0);
#endif
}

}  // namespace chrome
