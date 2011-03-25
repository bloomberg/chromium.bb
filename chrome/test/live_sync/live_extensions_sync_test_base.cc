// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test_base.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

LiveExtensionsSyncTestBase::LiveExtensionsSyncTestBase(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveExtensionsSyncTestBase::~LiveExtensionsSyncTestBase() {}

namespace {

// TODO(akalin): Somehow unify this with MakeExtension() in
// extension_util_unittest.cc.
scoped_refptr<Extension> CreateExtension(
    const ScopedTempDir& scoped_temp_dir,
    bool is_theme,
    int index) {
  DictionaryValue source;
  std::string name_prefix = is_theme ? "faketheme" : "fakeextension";
  source.SetString(
      extension_manifest_keys::kName,
      name_prefix + base::IntToString(index));
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  if (is_theme) {
    source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  }
  FilePath extension_dir;
  if (!file_util::CreateTemporaryDirInDir(
          scoped_temp_dir.path(), FILE_PATH_LITERAL("fakeextension"),
          &extension_dir)) {
    return NULL;
  }
  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(extension_dir,
                        Extension::INTERNAL, source, false, true, &error);
  if (!error.empty()) {
    LOG(WARNING) << error;
    return NULL;
  }
  return extension;
}

}  // namespace

bool LiveExtensionsSyncTestBase::SetupClients() {
  if (!LiveSyncTest::SetupClients())
    return false;

  for (int i = 0; i < num_clients(); ++i) {
    GetProfile(i)->InitExtensions();
  }
  verifier()->InitExtensions();

  if (!extension_base_dir_.CreateUniqueTempDir())
    return false;

  return true;
}

scoped_refptr<Extension> LiveExtensionsSyncTestBase::GetExtensionHelper(
    bool is_theme, int index) {
  std::pair<bool, int> type_index = std::make_pair(is_theme, index);
  ExtensionTypeIndexMap::const_iterator it =
      extensions_by_type_index_.find(type_index);
  if (it != extensions_by_type_index_.end()) {
    return it->second;
  }
  scoped_refptr<Extension> extension =
      CreateExtension(extension_base_dir_, is_theme, index);
  if (!extension.get())
    return NULL;
  extensions_by_type_index_[type_index] = extension;
  extensions_by_id_[extension->id()] = extension;
  return extension;
}

scoped_refptr<Extension> LiveExtensionsSyncTestBase::GetTheme(int index) {
  return GetExtensionHelper(true, index);
}

scoped_refptr<Extension> LiveExtensionsSyncTestBase::GetExtension(
    int index) {
  return GetExtensionHelper(false, index);
}

void LiveExtensionsSyncTestBase::InstallExtension(
    Profile* profile, scoped_refptr<Extension> extension) {
  CHECK(profile);
  CHECK(extension.get());
  profile->GetExtensionService()->OnExtensionInstalled(extension);
}

void LiveExtensionsSyncTestBase::InstallAllPendingExtensions(
    Profile* profile) {
  // TODO(akalin): Mock out the servers that the extensions auto-update
  // mechanism talk to so as to more closely match what actually happens.
  // Background networking will need to be re-enabled for extensions tests.

  // We make a copy here since InstallExtension() removes the
  // extension from the extensions service's copy.
  PendingExtensionMap pending_extensions =
      profile->GetExtensionService()->pending_extensions();
  for (PendingExtensionMap::const_iterator it = pending_extensions.begin();
       it != pending_extensions.end(); ++it) {
    ExtensionIdMap::const_iterator it2 = extensions_by_id_.find(it->first);
    CHECK(it2 != extensions_by_id_.end());
    InstallExtension(profile, it2->second);
  }
}
