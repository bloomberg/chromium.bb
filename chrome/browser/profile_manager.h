// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILE_MANAGER_H__
#define CHROME_BROWSER_PROFILE_MANAGER_H__

#include <map>
#include <string>
#include <vector>

#include "app/system_monitor.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/profile.h"

// This is a small storage class that simply represents some metadata about
// profiles that are available in the current user data directory.
// These are cached in local state so profiles don't need to be scanned
// for their metadata on every launch.
class AvailableProfile {
 public:
  AvailableProfile(const std::wstring& name,
                   const std::wstring& id,
                   const FilePath& directory)
      : name_(name), id_(id), directory_(directory) {}

  // Decodes a DictionaryValue into an AvailableProfile
  static AvailableProfile* FromValue(DictionaryValue* value) {
    DCHECK(value);
    std::wstring name, id;
    FilePath::StringType directory;
    value->GetString(L"name", &name);
    value->GetString(L"id", &id);
    value->GetString(L"directory", &directory);
    return new AvailableProfile(name, id, FilePath(directory));
  }

  // Encodes this AvailableProfile into a new DictionaryValue
  DictionaryValue* ToValue() {
    DictionaryValue* value = new DictionaryValue;
    value->SetString(L"name", name_);
    value->SetString(L"id", id_);
    value->SetString(L"directory", directory_.value());
    return value;
  }

  std::wstring name() const { return name_; }
  std::wstring id() const { return id_; }
  FilePath directory() const { return directory_; }

 private:
  std::wstring name_;  // User-visible profile name
  std::wstring id_;  // Profile identifier
  FilePath directory_;  // Subdirectory containing profile (not full path)

  DISALLOW_EVIL_CONSTRUCTORS(AvailableProfile);
};

class ProfileManager : public NonThreadSafe,
                       public SystemMonitor::PowerObserver {
 public:
  ProfileManager();
  virtual ~ProfileManager();

  // Invokes ShutdownSessionService() on all profiles.
  static void ShutdownSessionServices();

  // Returns the default profile.  This adds the profile to the
  // ProfileManager if it doesn't already exist.  This method returns NULL if
  // the profile doesn't exist and we can't create it.
  // The profile used can be overridden by using --profile on
  Profile* GetDefaultProfile(const FilePath& user_data_dir);

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  Profile* GetProfile(const FilePath& profile_dir);

  // These allow iteration through the current list of profiles.
  typedef std::vector<Profile*> ProfileVector;
  typedef ProfileVector::iterator iterator;
  typedef ProfileVector::const_iterator const_iterator;

  iterator begin() { return profiles_.begin(); }
  const_iterator begin() const { return profiles_.begin(); }
  iterator end() { return profiles_.end(); }
  const_iterator end() const { return profiles_.end(); }

  typedef std::vector<AvailableProfile*> AvailableProfileVector;

  // PowerObserver notifications
  void OnSuspend();
  void OnResume();

  // ------------------ static utility functions -------------------

  // Returns the path to the profile directory based on the user data directory.
  static FilePath GetDefaultProfileDir(const FilePath& user_data_dir);

// Returns the path to the preferences file given the user profile directory.
  static FilePath GetProfilePrefsPath(const FilePath& profile_dir);

  // Tries to determine whether the given path represents a profile
  // directory, and returns true if it thinks it does.
  static bool IsProfile(const FilePath& path);

  // If a profile with the given path is currently managed by this object,
  // return a pointer to the corresponding Profile object;
  // otherwise return NULL.
  Profile* GetProfileByPath(const FilePath& path) const;

  // Creates a new profile at the specified path.
  // This method should always return a valid Profile (i.e., should never
  // return NULL).
  static Profile* CreateProfile(const FilePath& path);

  // Adds a profile to the set of currently-loaded profiles.  Returns a
  // pointer to a Profile object corresponding to the given path.
  Profile* AddProfileByPath(const FilePath& path);

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager.  This ProfileManager takes ownership of the Profile.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(Profile* profile);

 private:
  // Hooks to suspend/resume per-profile network traffic.
  // These must be called on the IO thread.
  static void SuspendProfile(Profile*);
  static void ResumeProfile(Profile*);

  // We keep a simple vector of profiles rather than something fancier
  // because we expect there to be a small number of profiles active.
  ProfileVector profiles_;

  AvailableProfileVector available_profiles_;

  DISALLOW_EVIL_CONSTRUCTORS(ProfileManager);
};

#endif  // CHROME_BROWSER_PROFILE_MANAGER_H__
