// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/extensions_helper.h"

#include <cstring>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_installer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace extensions_helper {

const char extension_name_prefix[] = "fakeextension";

bool HasSameExtensionsAsVerifier(int index) {
  return SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
      test()->GetProfile(index), test()->verifier());
}

bool AllProfilesHaveSameExtensionsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!HasSameExtensionsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " doesn't have the same extensions as"
                                       " the verifier profile.";
      return false;
    }
  }
  return true;
}

bool AllProfilesHaveSameExtensions() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
        test()->GetProfile(0), test()->GetProfile(i))) {
      LOG(ERROR) << "Profile " << i << " doesnt have the same extensions as"
                                       " profile 0.";
      return false;
    }
  }
  return true;
}


std::string InstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeExtensionName(index),
      extensions::Manifest::TYPE_EXTENSION);
}

std::string InstallExtensionForAllProfiles(int index) {
  for (int i = 0; i < test()->num_clients(); ++i)
    InstallExtension(test()->GetProfile(i), index);
  return InstallExtension(test()->verifier(), index);
}

void UninstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeExtensionName(index));
}

std::vector<int> GetInstalledExtensions(Profile* profile) {
  std::vector<int> indices;
  std::vector<std::string> names =
      SyncExtensionHelper::GetInstance()->GetInstalledExtensionNames(profile);
  for (std::vector<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    int index;
    if (ExtensionNameToIndex(*it, &index)) {
      indices.push_back(index);
    }
  }
  return indices;
}

void EnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeExtensionName(index));
}

void DisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsExtensionEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsExtensionEnabled(
      profile, CreateFakeExtensionName(index));
}

void IncognitoEnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeExtensionName(index));
}

void IncognitoDisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsIncognitoEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsIncognitoEnabled(
      profile, CreateFakeExtensionName(index));
}

void InstallExtensionsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(profile);
}

std::string CreateFakeExtensionName(int index) {
  return extension_name_prefix + base::IntToString(index);
}

bool ExtensionNameToIndex(const std::string& name, int* index) {
  if (!StartsWithASCII(name, extension_name_prefix, true) ||
      !base::StringToInt(name.substr(strlen(extension_name_prefix)), index)) {
    LOG(WARNING) << "Unable to convert extension name \"" << name
                 << "\" to index";
    return false;
  }
  return true;
}

namespace {

// A helper class to implement waiting for a set of profiles to have matching
// extensions lists.
class ExtensionsMatchChecker : public StatusChangeChecker,
                               public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionsMatchChecker(const std::vector<Profile*>& profiles);
  virtual ~ExtensionsMatchChecker();

  // StatusChangeChecker implementation.
  virtual std::string GetDebugMessage() const OVERRIDE;
  virtual bool IsExitConditionSatisfied() OVERRIDE;

  // extensions::ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* context,
      const extensions::Extension* extenion,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  void Wait();

 private:
  std::vector<Profile*> profiles_;
  ScopedVector<SyncedExtensionInstaller> synced_extension_installers_;
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMatchChecker);
};

ExtensionsMatchChecker::ExtensionsMatchChecker(
    const std::vector<Profile*>& profiles)
    : profiles_(profiles), observing_(false) {
  DCHECK_GE(profiles_.size(), 2U);
}

ExtensionsMatchChecker::~ExtensionsMatchChecker() {
  if (observing_) {
    for (std::vector<Profile*>::iterator it = profiles_.begin();
         it != profiles_.end();
         ++it) {
      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(*it);
      registry->RemoveObserver(this);
    }
  }
}

std::string ExtensionsMatchChecker::GetDebugMessage() const {
  return "Waiting for extensions to match";
}

bool ExtensionsMatchChecker::IsExitConditionSatisfied() {
  std::vector<Profile*>::iterator it = profiles_.begin();
  Profile* profile0 = *it;
  ++it;
  for (; it != profiles_.end(); ++it) {
    if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(profile0,
                                                                  *it)) {
      return false;
    }
  }
  return true;
}

void ExtensionsMatchChecker::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionUnloaded(
    content::BrowserContext* context,
    const extensions::Extension* extenion,
    extensions::UnloadedExtensionInfo::Reason reason) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::Wait() {
  for (std::vector<Profile*>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    // Begin mocking the installation of synced extensions from the web store.
    synced_extension_installers_.push_back(new SyncedExtensionInstaller(*it));

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(*it);
    registry->AddObserver(this);
  }

  observing_ = true;

  if (IsExitConditionSatisfied()) {
    VLOG(1) << "Extensions matched without waiting";
    return;
  }

  VLOG(1) << "Starting Wait: " << GetDebugMessage();
  StartBlockingWait();
}

}  // namespace

bool AwaitAllProfilesHaveSameExtensionsAsVerifier() {
  std::vector<Profile*> profiles;
  profiles.push_back(test()->verifier());
  for (int i = 0; i < test()->num_clients(); ++i) {
    profiles.push_back(test()->GetProfile(i));
  }

  ExtensionsMatchChecker checker(profiles);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace extensions_helper
