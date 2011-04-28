// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/system_monitor/system_monitor.h"

class FilePath;

class ProfileManager : public base::NonThreadSafe,
                       public ui::SystemMonitor::PowerObserver,
                       public NotificationObserver,
                       public Profile::Delegate {
 public:
  class Observer {
   public:
    // This method is called when profile is ready. If profile creation has been
    // failed, method is called with |profile| equals to NULL.
    virtual void OnProfileCreated(Profile* profile) = 0;
  };

  ProfileManager();
  virtual ~ProfileManager();

  // Invokes ShutdownSessionService() on all profiles.
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

  // Explicit asynchronous creation of the profile. |observer| is called
  // when profile is created. If profile has already been created, observer
  // is called immediately. Should be called on the UI thread.
  void CreateProfileAsync(const FilePath& user_data_dir,
                          Observer* observer);

  // Initiates default profile creation. If default profile has already been
  // created, observer is called immediately. Should be called on the UI thread.
  static void CreateDefaultProfileAsync(Observer* observer);

  // Returns the profile with the given |profile_id| or NULL if no such profile
  // exists.
  Profile* GetProfileWithId(ProfileId profile_id);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(Profile* profile);

  // Returns the directory where the currently active profile is
  // stored, relative to the user data directory currently in use..
  FilePath GetCurrentProfileDir();

  // Returns created profiles. Note, profiles order is NOT guaranteed to be
  // related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // PowerObserver notifications
  virtual void OnSuspend();
  virtual void OnResume();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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

 protected:
  // Does final initial actions.
  virtual void DoFinalInit(Profile* profile);

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
    std::vector<Observer*> observers;

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

  NotificationRegistrar registrar_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_;

  // Maps profile path to ProfileInfo (if profile has been created). Use
  // RegisterProfile() to add into this map.
  typedef std::map<FilePath, linked_ptr<ProfileInfo> > ProfilesInfoMap;
  ProfilesInfoMap profiles_info_;

  DISALLOW_COPY_AND_ASSIGN(ProfileManager);
};

// Same as the ProfileManager, but doesn't initialize some services of the
// profile. This one is useful in unittests.
class ProfileManagerWithoutInit : public ProfileManager {
 protected:
  virtual void DoFinalInit(Profile*) {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
