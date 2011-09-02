// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_EXTENSION_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_EXTENSION_HELPER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/common/extensions/extension.h"

class Extension;
class LiveSyncTest;
class Profile;

class SyncExtensionHelper {
 public:
  // Singleton implementation.
  static SyncExtensionHelper* GetInstance();

  // Returns a generated extension ID for the given name.
  static std::string NameToId(const std::string& name);

  // Initializes the profiles in |test| and registers them with
  // internal data structures.
  void SetupIfNecessary(LiveSyncTest* test);

  // Installs the extension with the given name to |profile|.
  void InstallExtension(
      Profile* profile, const std::string& name, Extension::Type type);

  // Uninstalls the extension with the given name from |profile|.
  void UninstallExtension(Profile* profile, const std::string& name);

  // Returns a vector containing the names of all currently installed extensions
  // on |profile|.
  std::vector<std::string> GetInstalledExtensionNames(Profile* profile) const;

  // Enables the extension with the given name on |profile|.
  void EnableExtension(Profile* profile, const std::string& name);

  // Disables the extension with the given name on |profile|.
  void DisableExtension(Profile* profile, const std::string& name);

  // Returns true if the extension with the given name is enabled on |profile|.
  bool IsExtensionEnabled(Profile* profile, const std::string& name) const;

  // Enables the extension with the given name to run in incognito mode
  void IncognitoEnableExtension(Profile* profile, const std::string& name);

  // Disables the extension with the given name from running in incognito mode
  void IncognitoDisableExtension(Profile* profile, const std::string& name);

  // Returns true iff the extension is enabled in incognito mode on |profile|.
  bool IsIncognitoEnabled(Profile* profile, const std::string& name) const;

  // Returns true iff the extension with the given id is pending
  // install in |profile|.
  bool IsExtensionPendingInstallForSync(
      Profile* profile, const std::string& id) const;

  // Installs all extensions pending sync in |profile| of the given
  // type.
  void InstallExtensionsPendingForSync(Profile* profile, Extension::Type type);

  // Returns true iff |profile1| and |profile2| have the same extensions and
  // they are all in the same state.
  static bool ExtensionStatesMatch(Profile* profile1, Profile* profile2);

 private:
  struct ExtensionState {
    enum EnabledState { DISABLED, PENDING, ENABLED };

    ExtensionState();
    ~ExtensionState();
    bool Equals(const ExtensionState &other) const;

    EnabledState enabled_state;
    bool incognito_enabled;
  };

  typedef std::map<std::string, ExtensionState> ExtensionStateMap;
  typedef std::map<std::string, scoped_refptr<Extension> > ExtensionNameMap;
  typedef std::map<Profile*, ExtensionNameMap> ProfileExtensionNameMap;
  typedef std::map<std::string, std::string> StringMap;

  friend struct DefaultSingletonTraits<SyncExtensionHelper>;

  SyncExtensionHelper();
  ~SyncExtensionHelper();

  // Returns a map from |profile|'s installed extensions to their state.
  static ExtensionStateMap GetExtensionStates(Profile* profile);

  // Initializes extensions for |profile| and creates an entry in
  // |profile_extensions_| for it.
  void SetupProfile(Profile* profile);

  // Returns an extension for the given name in |profile|.  type and
  // index.  Two extensions with the name but different profiles will
  // have the same id.
  scoped_refptr<Extension> GetExtension(
      Profile* profile, const std::string& name,
      Extension::Type type) WARN_UNUSED_RESULT;

  ProfileExtensionNameMap profile_extensions_;
  StringMap id_to_name_;
  bool setup_completed_;

  DISALLOW_COPY_AND_ASSIGN(SyncExtensionHelper);
};

#endif  // CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_EXTENSION_HELPER_H_
