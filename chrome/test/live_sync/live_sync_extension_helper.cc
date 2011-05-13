// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_extension_helper.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetProfileName(Profile* profile) {
  const std::string& name = profile->GetPath().BaseName().MaybeAsASCII();
  EXPECT_FALSE(name.empty());
  return name;
}

}  // namespace

LiveSyncExtensionHelper::LiveSyncExtensionHelper() {}

LiveSyncExtensionHelper::~LiveSyncExtensionHelper() {}

// static
std::string LiveSyncExtensionHelper::NameToId(const std::string& name) {
  std::string id;
  EXPECT_TRUE(Extension::GenerateId(name, &id));
  return id;
}

void LiveSyncExtensionHelper::Setup(LiveSyncTest* test) {
  for (int i = 0; i < test->num_clients(); ++i) {
    SetupProfile(test->GetProfile(i));
  }
  SetupProfile(test->verifier());
}

void LiveSyncExtensionHelper::InstallExtension(
    Profile* profile, const std::string& name, Extension::Type type) {
  scoped_refptr<Extension> extension = GetExtension(profile, name, type);
  ASSERT_TRUE(extension.get()) << "Could not get extension " << name
                               << " (profile = " << profile << ")";
  profile->GetExtensionService()->OnExtensionInstalled(extension);
}

void LiveSyncExtensionHelper::UninstallExtension(
    Profile* profile, const std::string& name) {
  ExtensionService::UninstallExtensionHelper(profile->GetExtensionService(),
                                             NameToId(name));
}

bool LiveSyncExtensionHelper::IsExtensionPendingInstallForSync(
    Profile* profile, const std::string& id) const {
  const PendingExtensionManager* pending_extension_manager =
      profile->GetExtensionService()->pending_extension_manager();
  PendingExtensionInfo info;
  if (!pending_extension_manager->GetById(id, &info)) {
    return false;
  }
  return info.is_from_sync();
}

void LiveSyncExtensionHelper::InstallExtensionsPendingForSync(
    Profile* profile, Extension::Type type) {
  // TODO(akalin): Mock out the servers that the extensions auto-update
  // mechanism talk to so as to more closely match what actually happens.
  // Background networking will need to be re-enabled for extensions tests.

  // We make a copy here since InstallExtension() removes the
  // extension from the extensions service's copy.
  const PendingExtensionManager* pending_extension_manager =
      profile->GetExtensionService()->pending_extension_manager();
  PendingExtensionManager::PendingExtensionMap pending_extensions(
      pending_extension_manager->begin(),
      pending_extension_manager->end());
  for (PendingExtensionManager::const_iterator it = pending_extensions.begin();
       it != pending_extensions.end(); ++it) {
    if (!it->second.is_from_sync()) {
      continue;
    }
    const std::string& id = it->first;
    StringMap::const_iterator it2 = id_to_name_.find(id);
    if (it2 == id_to_name_.end()) {
      ADD_FAILURE() << "Could not get name for id " << id
                    << " (profile = " << GetProfileName(profile) << ")";
      continue;
    }
    InstallExtension(profile, it2->second, type);
  }
}

LiveSyncExtensionHelper::ExtensionStateMap
    LiveSyncExtensionHelper::GetExtensionStates(
        Profile* profile) const {
  const std::string& profile_name = GetProfileName(profile);

  ExtensionStateMap extension_state_map;

  ExtensionService* extension_service = profile->GetExtensionService();

  const ExtensionList* extensions = extension_service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    extension_state_map[(*it)->id()] = ENABLED;
    VLOG(2) << "Extension " << (*it)->id() << " in profile "
            << profile_name << " is enabled";
  }

  const ExtensionList* disabled_extensions =
      extension_service->disabled_extensions();
  for (ExtensionList::const_iterator it = disabled_extensions->begin();
       it != disabled_extensions->end(); ++it) {
    extension_state_map[(*it)->id()] = DISABLED;
    VLOG(2) << "Extension " << (*it)->id() << " in profile "
            << profile_name << " is disabled";
  }

  const PendingExtensionManager* pending_extension_manager =
      extension_service->pending_extension_manager();
  PendingExtensionManager::const_iterator it;
  for (it = pending_extension_manager->begin();
       it != pending_extension_manager->end(); ++it) {
    extension_state_map[it->first] = PENDING;
    VLOG(2) << "Extension " << it->first << " in profile "
            << profile_name << " is pending";
  }

  return extension_state_map;
}

void LiveSyncExtensionHelper::SetupProfile(Profile* profile) {
  profile->InitExtensions(true);
  profile_extensions_.insert(make_pair(profile, ExtensionNameMap()));
}

namespace {

std::string NameToPublicKey(const std::string& name) {
  std::string public_key;
  std::string pem;
  EXPECT_TRUE(Extension::ProducePEM(name, &pem) &&
              Extension::FormatPEMForFileOutput(pem, &public_key,
                                                true /* is_public */));
  return public_key;
}

// TODO(akalin): Somehow unify this with MakeExtension() in
// extension_util_unittest.cc.
scoped_refptr<Extension> CreateExtension(
    const FilePath& base_dir, const std::string& name,
    Extension::Type type) {
  DictionaryValue source;
  source.SetString(extension_manifest_keys::kName, name);
  const std::string& public_key = NameToPublicKey(name);
  source.SetString(extension_manifest_keys::kPublicKey, public_key);
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  switch (type) {
    case Extension::TYPE_EXTENSION:
      // Do nothing.
      break;
    case Extension::TYPE_THEME:
      source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
      break;
    case Extension::TYPE_HOSTED_APP:
    case Extension::TYPE_PACKAGED_APP:
      source.Set(extension_manifest_keys::kApp, new DictionaryValue());
      source.SetString(extension_manifest_keys::kLaunchWebURL,
                       "http://www.example.com");
      break;
    default:
      ADD_FAILURE();
      return NULL;
  }
  const FilePath sub_dir = FilePath().AppendASCII(name);
  FilePath extension_dir;
  if (!file_util::PathExists(base_dir) &&
      !file_util::CreateDirectory(base_dir) &&
      !file_util::CreateTemporaryDirInDir(
          base_dir, sub_dir.value(), &extension_dir)) {
    ADD_FAILURE();
    return NULL;
  }
  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(extension_dir, Extension::INTERNAL,
                        source, Extension::STRICT_ERROR_CHECKS, &error);
  if (!error.empty()) {
    ADD_FAILURE() << error;
    return NULL;
  }
  if (!extension.get()) {
    ADD_FAILURE();
    return NULL;
  }
  if (extension->name() != name) {
    EXPECT_EQ(name, extension->name());
    return NULL;
  }
  if (extension->GetType() != type) {
    EXPECT_EQ(type, extension->GetType());
    return NULL;
  }
  return extension;
}

}  // namespace

scoped_refptr<Extension> LiveSyncExtensionHelper::GetExtension(
    Profile* profile, const std::string& name,
    Extension::Type type) {
  if (name.empty()) {
    ADD_FAILURE();
    return NULL;
  }
  ProfileExtensionNameMap::iterator it = profile_extensions_.find(profile);
  if (it == profile_extensions_.end()) {
    ADD_FAILURE();
    return NULL;
  }
  ExtensionNameMap::const_iterator it2 = it->second.find(name);
  if (it2 != it->second.end()) {
    return it2->second;
  }

  scoped_refptr<Extension> extension =
      CreateExtension(profile->GetExtensionService()->install_directory(),
                      name, type);
  if (!extension.get()) {
    ADD_FAILURE();
    return NULL;
  }
  const std::string& expected_id = NameToId(name);
  if (extension->id() != expected_id) {
    EXPECT_EQ(expected_id, extension->id());
    return NULL;
  }
  VLOG(2) << "created extension with name = "
          << name << ", id = " << expected_id;
  (it->second)[name] = extension;
  id_to_name_[expected_id] = name;
  return extension;
}
