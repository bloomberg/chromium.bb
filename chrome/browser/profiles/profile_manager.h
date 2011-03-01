// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "ui/base/system_monitor/system_monitor.h"

class FilePath;

class ProfileManager : public base::NonThreadSafe,
                       public ui::SystemMonitor::PowerObserver,
                       public NotificationObserver {
 public:
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

  // Returns the profile with the given |profile_id| or NULL if no such profile
  // exists.
  Profile* GetProfileWithId(ProfileId profile_id);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(Profile* profile);

  // Returns a profile for a specific profile directory within the user data
  // dir with the option of controlling whether extensions are initialized
  // or not.  This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  // Note that if the profile has already been created, extensions may have
  // been initialized.  If this matters to you, you should call GetProfileByPath
  // first to see if the profile already exists.
  Profile* GetProfile(const FilePath& profile_dir, bool init_extensions);

  // Returns the directory where the currently active profile is
  // stored, relative to the user data directory currently in use..
  FilePath GetCurrentProfileDir();

  // These allow iteration through the current list of profiles.
  typedef std::vector<Profile*> ProfileVector;
  typedef ProfileVector::iterator iterator;
  typedef ProfileVector::const_iterator const_iterator;

  iterator begin() { return profiles_.begin(); }
  const_iterator begin() const { return profiles_.begin(); }
  iterator end() { return profiles_.end(); }
  const_iterator end() const { return profiles_.end(); }

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

 private:
  friend class ExtensionEventRouterForwarderTest;

  // Hooks to suspend/resume per-profile network traffic.
  // These must be called on the IO thread.
  static void SuspendProfile(Profile*);
  static void ResumeProfile(Profile*);

  // Helper method for unit tests to inject |profile| into the ProfileManager.
  void RegisterProfile(Profile* profile);

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager.  This ProfileManager takes ownership of the Profile.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(Profile* profile, bool init_extensions);

  // We keep a simple vector of profiles rather than something fancier
  // because we expect there to be a small number of profiles active.
  ProfileVector profiles_;

  NotificationRegistrar registrar_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_;

  DISALLOW_COPY_AND_ASSIGN(ProfileManager);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
