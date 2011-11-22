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
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_WIN)
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#endif

class NewProfileLauncher;
class ProfileInfoCache;

class ProfileManagerObserver {
 public:
  enum Status {
    // Asynchronous Profile services were not created.
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
                       public content::NotificationObserver,
                       public Profile::Delegate {
 public:
  explicit ProfileManager(const FilePath& user_data_dir);
  virtual ~ProfileManager();

  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();

  // Physically remove deleted profile directories from disk.
  static void NukeDeletedProfilesFromDisk();

  // DEPRECATED: DO NOT USE unless in ChromeOS.
  // Returns the default profile.  This adds the profile to the
  // ProfileManager if it doesn't already exist.  This method returns NULL if
  // the profile doesn't exist and we can't create it.
  // The profile used can be overridden by using --login-profile on cros.
  Profile* GetDefaultProfile(const FilePath& user_data_dir);

  // DEPRECATED: DO NOT USE unless in ChromeOS.
  // Same as instance method but provides the default user_data_dir as well.
  static Profile* GetDefaultProfile();

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  Profile* GetProfile(const FilePath& profile_dir);

  // Returns total number of profiles available on this machine.
  size_t GetNumberOfProfiles();

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

  // Returns created profiles. Note, profiles order is NOT guaranteed to be
  // related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // BrowserList::Observer implementation.
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(const Browser* browser) OVERRIDE;

  // Indicate that an import process will run for the next created Profile.
  void SetWillImport();
  bool will_import() { return will_import_; }

  // Indicate that the import process for |profile| has completed.
  void OnImportFinished(Profile* profile);

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

  // Opens a new window with the given profile. This launches a new browser for
  // the profile or activates an existing one; it is the static equivalent of
  // the instance method Browser::NewWindow(), used for the creation of a
  // Window from the multi-profile dropdown menu.
  static void NewWindowWithProfile(
      Profile* profile,
      BrowserInit::IsProcessStartup process_startup,
      BrowserInit::IsFirstRun is_first_run);

  // Profile::Delegate implementation:
  virtual void OnProfileCreated(Profile* profile, bool success) OVERRIDE;

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

  // Autoloads profiles if they are running background apps.
  void AutoloadProfiles();

  // Register and add testing profile to the ProfileManager. Use ONLY in tests.
  // This allows the creation of Profiles outside of the standard creation path
  // for testing. If |addToCache|, add to ProfileInfoCache as well.
  void RegisterTestingProfile(Profile* profile, bool addToCache);

#if defined(OS_WIN)
  // Remove the shortcut manager for testing.
  void RemoveProfileShortcutManagerForTesting();
#endif

  const FilePath& user_data_dir() const { return user_data_dir_; }

 protected:
  // Does final initial actions.
  virtual void DoFinalInit(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitForServices(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitLogging(Profile* profile);

  // Creates a new profile. Virtual so that unittests can return TestingProfile.
  virtual Profile* CreateProfile(const FilePath& path);

 private:
  friend class TestingProfileManager;
  FRIEND_TEST(ProfileManagerBrowserTest, DeleteAllProfiles);

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

  // Adds |profile| to the profile info cache if it hasn't been added yet.
  void AddProfileToCache(Profile* profile);

  // For ChromeOS, determines if profile should be otr.
  bool ShouldGoOffTheRecord();

  // Get the path of the next profile directory and increment the internal
  // count.
  // Lack of side effects:
  // This function doesn't actually create the directory or touch the file
  // system.
  FilePath GenerateNextProfileDirectoryPath();

  content::NotificationRegistrar registrar_;

  // The path to the user data directory (DIR_USER_DATA).
  const FilePath user_data_dir_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_;

  // True if an import process will be run.
  bool will_import_;

  // Maps profile path to ProfileInfo (if profile has been created). Use
  // RegisterProfile() to add into this map. This map owns all loaded profile
  // objects in a running instance of Chrome.
  typedef std::map<FilePath, linked_ptr<ProfileInfo> > ProfilesInfoMap;
  ProfilesInfoMap profiles_info_;

  // Object to cache various information about profiles. Contains information
  // about every profile which has been created for this instance of Chrome,
  // if it has not been explicitly deleted.
  scoped_ptr<ProfileInfoCache> profile_info_cache_;

#if defined(OS_WIN)
  // Manages the creation, deletion, and renaming of Windows shortcuts by
  // observing the ProfileInfoCache.
  scoped_ptr<ProfileShortcutManagerWin> profile_shortcut_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileManager);
};

// Same as the ProfileManager, but doesn't initialize some services of the
// profile. This one is useful in unittests.
class ProfileManagerWithoutInit : public ProfileManager {
 public:
  explicit ProfileManagerWithoutInit(const FilePath& user_data_dir);

 protected:
  virtual void DoFinalInitForServices(Profile*, bool) OVERRIDE {}
  virtual void DoFinalInitLogging(Profile*) OVERRIDE {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
