// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_extension_helper.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

SyncExtensionHelper::ExtensionState::ExtensionState()
    : enabled_state(ENABLED), incognito_enabled(false) {}

SyncExtensionHelper::ExtensionState::~ExtensionState() {}

bool SyncExtensionHelper::ExtensionState::Equals(
    const SyncExtensionHelper::ExtensionState &other) const {
  return ((enabled_state == other.enabled_state) &&
          (incognito_enabled == other.incognito_enabled));
}

// static
SyncExtensionHelper* SyncExtensionHelper::GetInstance() {
  SyncExtensionHelper* instance = Singleton<SyncExtensionHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncExtensionHelper::SyncExtensionHelper() : setup_completed_(false) {}

SyncExtensionHelper::~SyncExtensionHelper() {}

// static
std::string SyncExtensionHelper::NameToId(const std::string& name) {
  std::string id;
  EXPECT_TRUE(Extension::GenerateId(name, &id));
  return id;
}

void SyncExtensionHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_)
    return;

  for (int i = 0; i < test->num_clients(); ++i) {
    SetupProfile(test->GetProfile(i));
  }
  SetupProfile(test->verifier());

  setup_completed_ = true;
}

void SyncExtensionHelper::InstallExtension(
    Profile* profile, const std::string& name, Extension::Type type) {
  scoped_refptr<Extension> extension = GetExtension(profile, name, type);
  ASSERT_TRUE(extension.get()) << "Could not get extension " << name
                               << " (profile = " << profile << ")";
  profile->GetExtensionService()->OnExtensionInstalled(
      extension, extension->UpdatesFromGallery(), 0);
}

void SyncExtensionHelper::UninstallExtension(
    Profile* profile, const std::string& name) {
  ExtensionService::UninstallExtensionHelper(profile->GetExtensionService(),
                                             NameToId(name));
}

std::vector<std::string> SyncExtensionHelper::GetInstalledExtensionNames(
    Profile* profile) const {
  std::vector<std::string> names;
  ExtensionService* extension_service = profile->GetExtensionService();

  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    names.push_back((*it)->name());
  }

  const ExtensionSet* disabled_extensions =
      extension_service->disabled_extensions();
  for (ExtensionSet::const_iterator it = disabled_extensions->begin();
       it != disabled_extensions->end(); ++it) {
    names.push_back((*it)->name());
  }

  const ExtensionSet* terminated_extensions =
      extension_service->terminated_extensions();
  for (ExtensionSet::const_iterator it = terminated_extensions->begin();
       it != terminated_extensions->end(); ++it) {
    names.push_back((*it)->name());
  }

  return names;
}

void SyncExtensionHelper::EnableExtension(Profile* profile,
                                          const std::string& name) {
  profile->GetExtensionService()->EnableExtension(NameToId(name));
}

void SyncExtensionHelper::DisableExtension(Profile* profile,
                                           const std::string& name) {
  profile->GetExtensionService()->DisableExtension(NameToId(name));
}

bool SyncExtensionHelper::IsExtensionEnabled(
    Profile* profile, const std::string& name) const {
  return profile->GetExtensionService()->IsExtensionEnabled(NameToId(name));
}

void SyncExtensionHelper::IncognitoEnableExtension(
    Profile* profile, const std::string& name) {
  profile->GetExtensionService()->SetIsIncognitoEnabled(NameToId(name), true);
}

void SyncExtensionHelper::IncognitoDisableExtension(
    Profile* profile, const std::string& name) {
  profile->GetExtensionService()->SetIsIncognitoEnabled(NameToId(name), false);
}

bool SyncExtensionHelper::IsIncognitoEnabled(
    Profile* profile, const std::string& name) const {
  return profile->GetExtensionService()->IsIncognitoEnabled(NameToId(name));
}


bool SyncExtensionHelper::IsExtensionPendingInstallForSync(
    Profile* profile, const std::string& id) const {
  const PendingExtensionManager* pending_extension_manager =
      profile->GetExtensionService()->pending_extension_manager();
  PendingExtensionInfo info;
  if (!pending_extension_manager->GetById(id, &info)) {
    return false;
  }
  return info.is_from_sync();
}

void SyncExtensionHelper::InstallExtensionsPendingForSync(
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
                    << " (profile = " << profile->GetDebugName() << ")";
      continue;
    }
    InstallExtension(profile, it2->second, type);
  }
}

SyncExtensionHelper::ExtensionStateMap
    SyncExtensionHelper::GetExtensionStates(Profile* profile) {
  const std::string& profile_debug_name = profile->GetDebugName();

  ExtensionStateMap extension_state_map;

  ExtensionService* extension_service = profile->GetExtensionService();

  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const std::string& id = (*it)->id();
    extension_state_map[id].enabled_state = ExtensionState::ENABLED;
    extension_state_map[id].incognito_enabled =
        extension_service->IsIncognitoEnabled(id);
    DVLOG(2) << "Extension " << (*it)->id() << " in profile "
             << profile_debug_name << " is enabled";
  }

  const ExtensionSet* disabled_extensions =
      extension_service->disabled_extensions();
  for (ExtensionSet::const_iterator it = disabled_extensions->begin();
       it != disabled_extensions->end(); ++it) {
    const std::string& id = (*it)->id();
    extension_state_map[id].enabled_state = ExtensionState::DISABLED;
    extension_state_map[id].incognito_enabled =
        extension_service->IsIncognitoEnabled(id);
    DVLOG(2) << "Extension " << (*it)->id() << " in profile "
             << profile_debug_name << " is disabled";
  }

  const PendingExtensionManager* pending_extension_manager =
      extension_service->pending_extension_manager();
  PendingExtensionManager::const_iterator it;
  for (it = pending_extension_manager->begin();
       it != pending_extension_manager->end(); ++it) {
    const std::string& id = it->first;
    extension_state_map[id].enabled_state = ExtensionState::PENDING;
    extension_state_map[id].incognito_enabled =
        extension_service->IsIncognitoEnabled(id);
    DVLOG(2) << "Extension " << it->first << " in profile "
             << profile_debug_name << " is pending";
  }

  return extension_state_map;
}

bool SyncExtensionHelper::ExtensionStatesMatch(
    Profile* profile1, Profile* profile2) {
  const ExtensionStateMap& state_map1 = GetExtensionStates(profile1);
  const ExtensionStateMap& state_map2 = GetExtensionStates(profile2);
  if (state_map1.size() != state_map2.size()) {
    DVLOG(1) << "Number of extensions for profile " << profile1->GetDebugName()
             << " does not match profile " << profile2->GetDebugName();
    return false;
  }

  ExtensionStateMap::const_iterator it1 = state_map1.begin();
  ExtensionStateMap::const_iterator it2 = state_map2.begin();
  while (it1 != state_map1.end()) {
    if (it1->first != it2->first) {
      DVLOG(1) << "Extensions for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    } else if (!it1->second.Equals(it2->second)) {
      DVLOG(1) << "Extension states for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    }
    ++it1;
    ++it2;
  }
  return true;
}

void SyncExtensionHelper::SetupProfile(Profile* profile) {
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

scoped_refptr<Extension> SyncExtensionHelper::GetExtension(
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
  DVLOG(2) << "created extension with name = "
           << name << ", id = " << expected_id;
  (it->second)[name] = extension;
  id_to_name_[expected_id] = name;
  return extension;
}
