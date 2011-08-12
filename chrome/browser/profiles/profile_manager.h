// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#pragma once

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class FilePath;
class NewProfileLauncher;
class ProfileInfoCache;

class ProfileManagerObserver {
 public:
  enum Status {
    // So epic.
    STATUS_FAIL,
    // Profile created but before initializing extensions and promo resources.
    STATUS_CREATED,
    // Profile is created, extensions and promo resources are initialized.
    STATUS_INITIALIZED
  };

  // This method is called when profile is ready. If profile already exists,
  // method is called with pointer to that profile and STATUS_INITIALIZED.
  // If profile creation has failed, method is called with |profile| equal to
  // NULL and |status| equal to STATUS_FAIL. If profile has been created
  // successfully, method is called twice: first with STATUS_CREATED status
  // (before extensions are initialized) and eventually with STATUS_INITIALIZED.
  virtual void OnProfileCreated(Profile* profile, Status status) = 0;

  // If true, delete the observer after no more OnProfileCreated calls are
  // expected. Default is false.
  virtual bool DeleteAfter();

  virtual ~ProfileManagerObserver() {}
};

class ProfileManager : public base::NonThreadSafe,
                       public BrowserList::Observer,
                       public NotificationObserver,
                       public Profile::Delegate {
 public:
  ProfileManager();
  virtual ~ProfileManager();

  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();

  // Returns the default profile.  This adds the profile to the
  // ProfileManager if it doesn't already exist.  This method returns NULL if
  // the profile doesn't exist and we can't create it.
  // The profile used can be overridden by using --login-profile on cros.
  Profile* GetDefaultProfile(const FilePath& user_data_dir);

  // Same as instance method but provides the default user_data_dir as well.
  static Profile* GetDefaultProfile();

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  Profile* GetProfile(const FilePath& profile_dir);

  // Multi-profile support.
  size_t GetNumberOfProfiles();
  string16 GetNameOfProfileAtIndex(size_t index);
  FilePath GetFilePathOfProfileAtIndex(size_t index,
                                       const FilePath& user_data_dir);

  // Explicit asynchronous creation of the profile. |observer| is called
  // when profile is created. If profile has already been created, observer
  // is called immediately. Should be called on the UI thread.
  void CreateProfileAsync(const FilePath& user_data_dir,
                          ProfileManagerObserver* observer);

  // Initiates default profile creation. If default profile has already been
  // created, observer is called immediately. Should be called on the UI thread.
  static void CreateDefaultProfileAsync(ProfileManagerObserver* observer);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(Profile* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use..
  FilePath GetInitialProfileDir();

  // Get the Profile last used with this Chrome build. If no signed profile has
  // been stored in Local State, hand back the Default profile.
  Profile* GetLastUsedProfile(const FilePath& user_data_dir);

  // Same as instance method but provides the default user_data_dir as well.
  static Profile* GetLastUsedProfile();

  // Register the mapping of a directory to a profile name in Local State.
  void RegisterProfileName(Profile* profile);

  // Returns created profiles. Note, profiles order is NOT guaranteed to be
  // related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // BrowserList::Observer implementation.
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoved(const Browser* browser);
  virtual void OnBrowserSetLastActive(const Browser* browser);

  // ------------------ static utility functions -------------------

  // Returns the path to the default profile directory, based on the given
  // user data directory.
  static FilePath GetDefaultProfileDir(const FilePath& user_data_dir);

  // Returns the path to the preferences file given the user profile directory.
  static FilePath GetProfilePrefsPath(const FilePath& profile_dir);

  // If a profile with the given path is currently managed by this object,
  // return a pointer to the corresponding Profile object;
  // otherwise return NULL.
  Profile* GetProfileByPath(const FilePath& path) const;

  // Profile::Delegate implementation:
  virtual void OnProfileCreated(Profile* profile, bool success);

  // Add or remove a profile launcher to/from the list of launchers waiting for
  // new profiles to be created from the multi-profile menu.
  void AddProfileLauncher(NewProfileLauncher* profile_launcher);
  void RemoveProfileLauncher(NewProfileLauncher* profile_launcher);

  // Creates a new profile in the next available multiprofile directory.
  // Directories are named "profile_1", "profile_2", etc., in sequence of
  // creation. (Because directories can be removed, however, it may be the case
  // that at some point the list of numbered profiles is not continuous.)
  static void CreateMultiProfileAsync();

  // Register multi-profile related preferences in Local State.
  static void RegisterPrefs(PrefService* prefs);

  // Returns a ProfileInfoCache object which can be used to get information
  // about profiles without having to load them from disk.
  ProfileInfoCache& GetProfileInfoCache();

  // Schedules the profile at the given path to be deleted on shutdown.
  void ScheduleProfileForDeletion(const FilePath& profile_dir);

  // Checks if multiple profiles is enabled.
  static bool IsMultipleProfilesEnabled();

 protected:
  // Does final initial actions.
  virtual void DoFinalInit(Profile* profile, bool go_off_the_record);

 private:
  friend class ExtensionEventRouterForwarderTest;

  // This struct contains information about profiles which are being loaded or
  // were loaded.
  struct ProfileInfo {
    ProfileInfo(Profile* profile, bool created)
        : profile(profile), created(created) {
    }

    ~ProfileInfo() {}

    scoped_ptr<Profile> profile;
    // Whether profile has been fully loaded (created and initialized).
    bool created;
    // List of observers which should be notified when profile initialization is
    // done. Note, when profile is fully loaded this vector will be empty.
    std::vector<ProfileManagerObserver*> observers;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProfileInfo);
  };

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager. This ProfileManager takes ownership of the Profile.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(Profile* profile);

  // Registers profile with given info. Returns pointer to created ProfileInfo
  // entry.
  ProfileInfo* RegisterProfile(Profile* profile, bool created);

  typedef std::pair<FilePath, string16> ProfilePathAndName;
  typedef std::vector<ProfilePathAndName> ProfilePathAndNames;
  ProfilePathAndNames GetSortedProfilesFromDirectoryMap();

  static bool CompareProfilePathAndName(
      const ProfileManager::ProfilePathAndName& pair1,
      const ProfileManager::ProfilePathAndName& pair2);

  // Adds |profile| to the profile info cache if it's not already there.
  void AddProfileToCache(Profile* profile);

  // For ChromeOS, determines if profile should be otr.
  bool ShouldGoOffTheRecord();

  NotificationRegistrar registrar_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_;

  // Maps profile path to ProfileInfo (if profile has been created). Use
  // RegisterProfile() to add into this map.
  typedef std::map<FilePath, linked_ptr<ProfileInfo> > ProfilesInfoMap;
  ProfilesInfoMap profiles_info_;

  // Object to cache various information about profiles.
  scoped_ptr<ProfileInfoCache> profile_info_cache_;

  // Profiles that should be deleted on shutdown.
  std::vector<FilePath> profiles_to_delete_;

  DISALLOW_COPY_AND_ASSIGN(ProfileManager);
};

// Same as the ProfileManager, but doesn't initialize some services of the
// profile. This one is useful in unittests.
class ProfileManagerWithoutInit : public ProfileManager {
 protected:
  virtual void DoFinalInit(Profile*, bool) {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
