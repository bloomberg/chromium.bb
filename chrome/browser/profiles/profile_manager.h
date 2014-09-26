// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class NewProfileLauncher;
class ProfileInfoCache;

class ProfileManager : public base::NonThreadSafe,
                       public content::NotificationObserver,
                       public Profile::Delegate {
 public:
  typedef base::Callback<void(Profile*, Profile::CreateStatus)> CreateCallback;

  explicit ProfileManager(const base::FilePath& user_data_dir);
  virtual ~ProfileManager();

#if defined(ENABLE_SESSION_SERVICE)
  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();
#endif

  // Physically remove deleted profile directories from disk.
  static void NukeDeletedProfilesFromDisk();

  // Same as instance method but provides the default user_data_dir as well.
  // If the Profile is going to be used to open a new window then consider using
  // GetLastUsedProfileAllowedByPolicy() instead.
  static Profile* GetLastUsedProfile();

  // Same as GetLastUsedProfile() but returns the incognito Profile if
  // incognito mode is forced. This should be used if the last used Profile
  // will be used to open new browser windows.
  static Profile* GetLastUsedProfileAllowedByPolicy();

  // Same as instance method but provides the default user_data_dir as well.
  static std::vector<Profile*> GetLastOpenedProfiles();

  // Get the profile for the user which created the current session.
  // Note that in case of a guest account this will return a 'suitable' profile.
  // This function is temporary and will soon be moved to ash. As such avoid
  // using it at all cost.
  // TODO(skuhne): Move into ash's new user management function.
  static Profile* GetPrimaryUserProfile();

  // Get the profile for the currently active user.
  // Note that in case of a guest account this will return a 'suitable' profile.
  // This function is temporary and will soon be moved to ash. As such avoid
  // using it at all cost.
  // TODO(skuhne): Move into ash's new user management function.
  static Profile* GetActiveUserProfile();

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  Profile* GetProfile(const base::FilePath& profile_dir);

  // Returns total number of profiles available on this machine.
  size_t GetNumberOfProfiles();

  // Explicit asynchronous creation of a profile located at |profile_path|.
  // If the profile has already been created then callback is called
  // immediately. Should be called on the UI thread.
  void CreateProfileAsync(const base::FilePath& profile_path,
                          const CreateCallback& callback,
                          const base::string16& name,
                          const base::string16& icon_url,
                          const std::string& supervised_user_id);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(Profile* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use.
  base::FilePath GetInitialProfileDir();

  // Get the Profile last used (the Profile to which owns the most recently
  // focused window) with this Chrome build. If no signed profile has been
  // stored in Local State, hand back the Default profile.
  Profile* GetLastUsedProfile(const base::FilePath& user_data_dir);

  // Get the path of the last used profile, or if that's undefined, the default
  // profile.
  base::FilePath GetLastUsedProfileDir(const base::FilePath& user_data_dir);

  // Get the Profiles which are currently open, i.e., have open browsers, or
  // were open the last time Chrome was running. The Profiles appear in the
  // order they were opened. The last used profile will be on the list, but its
  // index on the list will depend on when it was opened (it is not necessarily
  // the last one).
  std::vector<Profile*> GetLastOpenedProfiles(
      const base::FilePath& user_data_dir);

  // Returns created profiles. Note, profiles order is NOT guaranteed to be
  // related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // If a profile with the given path is currently managed by this object,
  // return a pointer to the corresponding Profile object;
  // otherwise return NULL.
  Profile* GetProfileByPath(const base::FilePath& path) const;

  // Creates a new profile in the next available multiprofile directory.
  // Directories are named "profile_1", "profile_2", etc., in sequence of
  // creation. (Because directories can be removed, however, it may be the case
  // that at some point the list of numbered profiles is not continuous.)
  // |callback| may be invoked multiple times (for CREATE_STATUS_INITIALIZED
  // and CREATE_STATUS_CREATED) so binding parameters with bind::Passed() is
  // prohibited. Returns the file path to the profile that will be created
  // asynchronously.
  static base::FilePath CreateMultiProfileAsync(
      const base::string16& name,
      const base::string16& icon_url,
      const CreateCallback& callback,
      const std::string& supervised_user_id);

  // Returns the full path to be used for guest profiles.
  static base::FilePath GetGuestProfilePath();

  // Get the path of the next profile directory and increment the internal
  // count.
  // Lack of side effects:
  // This function doesn't actually create the directory or touch the file
  // system.
  base::FilePath GenerateNextProfileDirectoryPath();

  // Returns a ProfileInfoCache object which can be used to get information
  // about profiles without having to load them from disk.
  ProfileInfoCache& GetProfileInfoCache();

  // Returns a ProfileShortcut Manager that enables the caller to create
  // profile specfic desktop shortcuts.
  ProfileShortcutManager* profile_shortcut_manager();

  // Schedules the profile at the given path to be deleted on shutdown. If we're
  // deleting the last profile, a new one will be created in its place, and in
  // that case the callback will be called when profile creation is complete.
  void ScheduleProfileForDeletion(const base::FilePath& profile_dir,
                                  const CreateCallback& callback);

  // Called on start-up if there are any stale ephemeral profiles to be deleted.
  // This can be the case if the browser has crashed and the clean-up code had
  // no chance to run then.
  static void CleanUpStaleProfiles(
      const std::vector<base::FilePath>& profile_paths);

  // Autoloads profiles if they are running background apps.
  void AutoloadProfiles();

  // Initializes user prefs of |profile|. This includes profile name and
  // avatar values.
  void InitProfileUserPrefs(Profile* profile);

  // Register and add testing profile to the ProfileManager. Use ONLY in tests.
  // This allows the creation of Profiles outside of the standard creation path
  // for testing. If |addToCache|, adds to ProfileInfoCache as well.
  // If |start_deferred_task_runners|, starts the deferred task runners.
  // Use ONLY in tests.
  void RegisterTestingProfile(Profile* profile,
                              bool addToCache,
                              bool start_deferred_task_runners);

  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  // For ChromeOS, determines if the user has logged in to a real profile.
  bool IsLoggedIn() const { return logged_in_; }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Profile::Delegate implementation:
  virtual void OnProfileCreated(Profile* profile,
                                bool success,
                                bool is_new_profile) OVERRIDE;

 protected:
  // Does final initial actions.
  virtual void DoFinalInit(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitForServices(Profile* profile, bool go_off_the_record);
  virtual void DoFinalInitLogging(Profile* profile);

  // Creates a new profile by calling into the profile's profile creation
  // method. Virtual so that unittests can return a TestingProfile instead
  // of the Profile's result.
  virtual Profile* CreateProfileHelper(const base::FilePath& path);

  // Creates a new profile asynchronously by calling into the profile's
  // asynchronous profile creation method. Virtual so that unittests can return
  // a TestingProfile instead of the Profile's result.
  virtual Profile* CreateProfileAsyncHelper(const base::FilePath& path,
                                            Delegate* delegate);

 private:
  friend class TestingProfileManager;
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, DeleteAllProfiles);
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, SwitchToProfile);

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

  // Returns the profile of the active user and / or the off the record profile
  // if needed. This adds the profile to the ProfileManager if it doesn't
  // already exist. The method will return NULL if the profile doesn't exist
  // and we can't create it.
  // The profile used can be overridden by using --login-profile on cros.
  Profile* GetActiveUserOrOffTheRecordProfileFromPath(
               const base::FilePath& user_data_dir);

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager. This ProfileManager takes ownership of the Profile.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(Profile* profile);

  // Schedules the profile at the given path to be deleted on shutdown.
  void FinishDeletingProfile(const base::FilePath& profile_dir);

  // Registers profile with given info. Returns pointer to created ProfileInfo
  // entry.
  ProfileInfo* RegisterProfile(Profile* profile, bool created);

  // Returns ProfileInfo associated with given |path|, registred earlier with
  // RegisterProfile.
  ProfileInfo* GetProfileInfoByPath(const base::FilePath& path) const;

  // Adds |profile| to the profile info cache if it hasn't been added yet.
  void AddProfileToCache(Profile* profile);

  // Apply settings for (desktop) Guest User profile.
  void SetGuestProfilePrefs(Profile* profile);

  // For ChromeOS, determines if profile should be otr.
  bool ShouldGoOffTheRecord(Profile* profile);

  void RunCallbacks(const std::vector<CreateCallback>& callbacks,
                    Profile* profile,
                    Profile::CreateStatus status);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Updates the last active user of the current session.
  // On Chrome OS updating this user will have no effect since when browser is
  // restored after crash there's another preference that is taken into account.
  // See kLastActiveUser in UserManagerBase.
  void UpdateLastUser(Profile* last_active);

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
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(OS_MACOSX)
  // If the |loaded_profile| has been loaded successfully (according to
  // |status|) and isn't already scheduled for deletion, then finishes adding
  // |profile_to_delete_dir| to the queue of profiles to be deleted, and updates
  // the kProfileLastUsed preference based on
  // |last_non_supervised_profile_path|.
  void OnNewActiveProfileLoaded(
      const base::FilePath& profile_to_delete_path,
      const base::FilePath& last_non_supervised_profile_path,
      const CreateCallback& original_callback,
      Profile* loaded_profile,
      Profile::CreateStatus status);
#endif

  content::NotificationRegistrar registrar_;

  // The path to the user data directory (DIR_USER_DATA).
  const base::FilePath user_data_dir_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  BrowserListObserver browser_list_observer_;
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

  // Maps profile path to ProfileInfo (if profile has been created). Use
  // RegisterProfile() to add into this map. This map owns all loaded profile
  // objects in a running instance of Chrome.
  typedef std::map<base::FilePath, linked_ptr<ProfileInfo> > ProfilesInfoMap;
  ProfilesInfoMap profiles_info_;

  // Object to cache various information about profiles. Contains information
  // about every profile which has been created for this instance of Chrome,
  // if it has not been explicitly deleted.
  scoped_ptr<ProfileInfoCache> profile_info_cache_;

  // Manages the process of creating, deleteing and updating Desktop shortcuts.
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;

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
  explicit ProfileManagerWithoutInit(const base::FilePath& user_data_dir);

 protected:
  virtual void DoFinalInitForServices(Profile*, bool) OVERRIDE {}
  virtual void DoFinalInitLogging(Profile*) OVERRIDE {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
