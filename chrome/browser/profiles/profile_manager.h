// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class NewProfileLauncher;
class ProfileInfoCache;

class ProfileManager : public base::NonThreadSafe,
                       public content::NotificationObserver,
                       public Profile::Delegate {
 public:
  typedef base::Callback<void(Profile*, Profile::CreateStatus)> CreateCallback;

  explicit ProfileManager(const FilePath& user_data_dir);
  virtual ~ProfileManager();

#if defined(ENABLE_SESSION_SERVICE)
  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();
#endif

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

  // DEPRECATED: DO NOT USE unless in ChromeOS.
  // Same as GetDefaultProfile() but returns OffTheRecord profile
  // if guest login.
  static Profile* GetDefaultProfileOrOffTheRecord();

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  Profile* GetProfile(const FilePath& profile_dir);

  // Returns total number of profiles available on this machine.
  size_t GetNumberOfProfiles();

  // Explicit asynchronous creation of a profile located at |profile_path|.
  // If the profile has already been created then callback is called
  // immediately. Should be called on the UI thread.
  void CreateProfileAsync(const FilePath& profile_path,
                          const CreateCallback& callback,
                          const string16& name,
                          const string16& icon_url);

  // Initiates default profile creation. If default profile has already been
  // created then the callback is called immediately. Should be called on the
  // UI thread.
  static void CreateDefaultProfileAsync(const CreateCallback& callback);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(Profile* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use..
  FilePath GetInitialProfileDir();

  // Get the Profile last used (the Profile to which owns the most recently
  // focused window) with this Chrome build. If no signed profile has been
  // stored in Local State, hand back the Default profile.
  Profile* GetLastUsedProfile(const FilePath& user_data_dir);

  // Same as instance method but provides the default user_data_dir as well.
  static Profile* GetLastUsedProfile();

  // Get the Profiles which are currently open, i.e., have open browsers, or
  // were open the last time Chrome was running. The Profiles appear in the
  // order they were opened. The last used profile will be on the list, but its
  // index on the list will depend on when it was opened (it is not necessarily
  // the last one).
  std::vector<Profile*> GetLastOpenedProfiles(const FilePath& user_data_dir);

  // Same as instance method but provides the default user_data_dir as well.
  static std::vector<Profile*> GetLastOpenedProfiles();

  // Returns created profiles. Note, profiles order is NOT guaranteed to be
  // related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  // Activates a window for |profile|.  If no such window yet exists, or if
  // |always_create| is true, this first creates a new window, then activates
  // that. If activating an exiting window and multiple windows exists then the
  // window that was most recently active is activated. This is used for
  // creation of a window from the multi-profile dropdown menu.
  static void FindOrCreateNewWindowForProfile(
      Profile* profile,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      bool always_create);

  // Profile::Delegate implementation:
  virtual void OnProfileCreated(Profile* profile,
                                bool success,
                                bool is_new_profile) OVERRIDE;

  // Add or remove a profile launcher to/from the list of launchers waiting for
  // new profiles to be created from the multi-profile menu.
  void AddProfileLauncher(NewProfileLauncher* profile_launcher);
  void RemoveProfileLauncher(NewProfileLauncher* profile_launcher);

  // Creates a new profile in the next available multiprofile directory.
  // Directories are named "profile_1", "profile_2", etc., in sequence of
  // creation. (Because directories can be removed, however, it may be the case
  // that at some point the list of numbered profiles is not continuous.)
  static void CreateMultiProfileAsync(
      const string16& name,
      const string16& icon_url,
      const CreateCallback& callback);

  // Register multi-profile related preferences in Local State.
  static void RegisterPrefs(PrefService* prefs);

  // Returns a ProfileInfoCache object which can be used to get information
  // about profiles without having to load them from disk.
  ProfileInfoCache& GetProfileInfoCache();

  // Returns a ProfileShortcut Manager that enables the caller to create
  // profile specfic desktop shortcuts.
  ProfileShortcutManager* profile_shortcut_manager();

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

  const FilePath& user_data_dir() const { return user_data_dir_; }

 protected:
  // Does final initial actions.
  virtual void DoFinalInit(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitForServices(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitLogging(Profile* profile);

  // Creates a new profile by calling into the profile's profile creation
  // method. Virtual so that unittests can return a TestingProfile instead
  // of the Profile's result.
  virtual Profile* CreateProfileHelper(const FilePath& path);

  // Creates a new profile asynchronously by calling into the profile's
  // asynchronous profile creation method. Virtual so that unittests can return
  // a TestingProfile instead of the Profile's result.
  virtual Profile* CreateProfileAsyncHelper(const FilePath& path,
                                            Delegate* delegate);

 private:
  friend class TestingProfileManager;
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, DeleteAllProfiles);

  // This struct contains information about profiles which are being loaded or
  // were loaded.
  struct ProfileInfo {
    ProfileInfo(Profile* profile, bool created);

    ~ProfileInfo();

    scoped_ptr<Profile> profile;
    // Whether profile has been fully loaded (created and initialized).
    bool created;
    // Whether or not this profile should have a shortcut.
    bool create_shortcut;
    // List of callbacks to run when profile initialization is done. Note, when
    // profile is fully loaded this vector will be empty.
    std::vector<CreateCallback> callbacks;

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

  // Initializes user prefs of |profile|. This includes profile name and
  // avatar values
  void InitProfileUserPrefs(Profile* profile);

  // For ChromeOS, determines if profile should be otr.
  bool ShouldGoOffTheRecord();

  // Get the path of the next profile directory and increment the internal
  // count.
  // Lack of side effects:
  // This function doesn't actually create the directory or touch the file
  // system.
  FilePath GenerateNextProfileDirectoryPath();

  void RunCallbacks(const std::vector<CreateCallback>& callbacks,
                    Profile* profile,
                    Profile::CreateStatus status);

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

  // Manages the process of creating, deleteing and updating Desktop shortcuts.
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;

#if !defined(OS_ANDROID)
  class BrowserListObserver : public chrome::BrowserListObserver {
   public:
    explicit BrowserListObserver(ProfileManager* manager);
    virtual ~BrowserListObserver();

    // chrome::BrowserListObserver implementation.
    virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
    virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;
    virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

   private:
    ProfileManager* profile_manager_;
    DISALLOW_COPY_AND_ASSIGN(BrowserListObserver);
  };

  BrowserListObserver browser_list_observer_;
#endif  // !defined(OS_ANDROID)

  // For keeping track of the last active profiles.
  std::map<Profile*, int> browser_counts_;
  // On startup we launch the active profiles in the order they became active
  // during the last run. This is why they are kept in a list, not in a set.
  std::vector<Profile*> active_profiles_;
  bool closing_all_browsers_;

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
