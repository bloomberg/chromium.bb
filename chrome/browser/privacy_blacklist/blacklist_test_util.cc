// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_test_util.h"

#include "base/file_path.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

BlacklistTestingProfile::BlacklistTestingProfile() {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  path_ = temp_dir_.path();
}

TestBlacklistPathProvider::TestBlacklistPathProvider(Profile* profile)
    : profile_(profile) {
}

bool TestBlacklistPathProvider::AreBlacklistPathsReady() const {
  return true;
}

std::vector<FilePath> TestBlacklistPathProvider::GetPersistentBlacklistPaths() {
  return persistent_paths_;
}

std::vector<FilePath> TestBlacklistPathProvider::GetTransientBlacklistPaths() {
  return transient_paths_;
}

void TestBlacklistPathProvider::AddPersistentPath(const FilePath& path) {
  persistent_paths_.push_back(path);
  SendUpdateNotification();
}

void TestBlacklistPathProvider::AddTransientPath(const FilePath& path) {
  transient_paths_.push_back(path);
  SendUpdateNotification();
}

void TestBlacklistPathProvider::clear() {
  persistent_paths_.clear();
  transient_paths_.clear();
  SendUpdateNotification();
}

void TestBlacklistPathProvider::SendUpdateNotification() {
  ListValue* privacy_blacklists = new ListValue;
  privacy_blacklists->Append(new StringValue("dummy.pbl"));

  DictionaryValue manifest;
  manifest.SetString(extension_manifest_keys::kVersion, "1.0");
  manifest.SetString(extension_manifest_keys::kName, "test");
  manifest.Set(extension_manifest_keys::kPrivacyBlacklists,
               privacy_blacklists);

  // Create an extension with dummy path.
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;
  ASSERT_TRUE(extension.InitFromValue(manifest, false, &error));
  ASSERT_TRUE(error.empty());

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_LOADED,
      Source<Profile>(profile_),
      Details<Extension>(&extension));
}
