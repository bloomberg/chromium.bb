// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#endif

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->ShutdownSessionService();
}

// static
Profile* ProfileManager::GetDefaultProfile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

ProfileManager::ProfileManager() : logged_in_(false) {
  ui::SystemMonitor::Get()->AddObserver(this);
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_CHANGED,
      NotificationService::AllSources());
#endif
}

ProfileManager::~ProfileManager() {
  ui::SystemMonitor* system_monitor = ui::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveObserver(this);

  // Destroy all profiles that we're keeping track of.
  for (const_iterator i(begin()); i != end(); ++i)
    delete *i;
  profiles_.clear();
}

FilePath ProfileManager::GetDefaultProfileDir(
    const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(
      FilePath::FromWStringHack(chrome::kNotSignedInProfile));
  return default_profile_dir;
}

FilePath ProfileManager::GetProfilePrefsPath(
    const FilePath &profile_dir) {
  FilePath default_prefs_path(profile_dir);
  default_prefs_path = default_prefs_path.Append(chrome::kPreferencesFilename);
  return default_prefs_path;
}

FilePath ProfileManager::GetCurrentProfileDir() {
  FilePath relative_profile_dir;
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (logged_in_) {
    FilePath profile_dir;
    // If the user has logged in, pick up the new profile.
    if (command_line.HasSwitch(switches::kLoginProfile)) {
      profile_dir = command_line.GetSwitchValuePath(switches::kLoginProfile);
    } else {
      // We should never be logged in with no profile dir.
      NOTREACHED();
      return FilePath("");
    }
    relative_profile_dir = relative_profile_dir.Append(profile_dir);
    return relative_profile_dir;
  }
#endif
  relative_profile_dir = relative_profile_dir.Append(
      FilePath::FromWStringHack(chrome::kNotSignedInProfile));
  return relative_profile_dir;
}

Profile* ProfileManager::GetDefaultProfile(const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(GetCurrentProfileDir());
#if defined(OS_CHROMEOS)
  if (!logged_in_) {
    Profile* profile;
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();

    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    // TODO(davemoore) Fix the tests so they allow OTR profiles.
    if (!command_line.HasSwitch(switches::kTestType) ||
        command_line.HasSwitch(switches::kLoginProfile)) {
      // Don't init extensions for this profile
      profile = GetProfile(default_profile_dir, false);
      profile = profile->GetOffTheRecordProfile();
    } else {
      profile = GetProfile(default_profile_dir, true);
    }
    return profile;
  }
#endif
  return GetProfile(default_profile_dir);
}

Profile* ProfileManager::GetProfile(const FilePath& profile_dir) {
  return GetProfile(profile_dir, true);
}

Profile* ProfileManager::GetProfile(
    const FilePath& profile_dir, bool init_extensions) {
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (NULL != profile)
    return profile;

  if (!ProfileManager::IsProfile(profile_dir)) {
    // If the profile directory doesn't exist, create it.
    profile = ProfileManager::CreateProfile(profile_dir);
  } else {
    // The profile already exists on disk, just load it.
    profile = Profile::CreateProfile(profile_dir);
  }
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile, init_extensions);
    DCHECK(result);
  }
  return profile;
}

bool ProfileManager::AddProfile(Profile* profile, bool init_extensions) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  profiles_.insert(profiles_.end(), profile);
  if (init_extensions)
    profile->InitExtensions();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitPromoResources();
  return true;
}

Profile* ProfileManager::GetProfileByPath(const FilePath& path) const {
  for (const_iterator i(begin()); i != end(); ++i) {
    if ((*i)->GetPath() == path)
      return *i;
  }

  return NULL;
}

void ProfileManager::OnSuspend() {
  DCHECK(CalledOnValidThread());

  for (const_iterator i(begin()); i != end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&ProfileManager::SuspendProfile, *i));
  }
}

void ProfileManager::OnResume() {
  DCHECK(CalledOnValidThread());
  for (const_iterator i(begin()); i != end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&ProfileManager::ResumeProfile, *i));
  }
}

void ProfileManager::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == NotificationType::LOGIN_USER_CHANGED) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType)) {
      // This will fail when running on non cros os.
      // TODO(davemoore) Need to mock this enough to enable testing.
      CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
      // If we don't have a mounted profile directory we're in trouble.
      // TODO(davemoore) Once we have better api this check should ensure that
      // our profile directory is the one that's mounted, and that it's mounted
      // as the current user.
      CHECK(chromeos::CrosLibrary::Get()->GetCryptohomeLibrary()->IsMounted());
    }
    logged_in_ = true;
  }
#endif
}

void ProfileManager::SuspendProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (net::URLRequestJobTracker::JobIterator i =
           net::g_url_request_job_tracker.begin();
       i != net::g_url_request_job_tracker.end(); ++i)
    (*i)->Kill();

  profile->GetRequestContext()->GetURLRequestContext()->
      http_transaction_factory()->Suspend(true);
}

void ProfileManager::ResumeProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  profile->GetRequestContext()->GetURLRequestContext()->
      http_transaction_factory()->Suspend(false);
}

// static
bool ProfileManager::IsProfile(const FilePath& path) {
  FilePath prefs_path = GetProfilePrefsPath(path);
  FilePath history_path = path;
  history_path = history_path.Append(chrome::kHistoryFilename);

  return file_util::PathExists(prefs_path) &&
      file_util::PathExists(history_path);
}

// static
Profile* ProfileManager::CreateProfile(const FilePath& path) {
  if (IsProfile(path)) {
    DCHECK(false) << "Attempted to create a profile with the path:\n"
        << path.value() << "\n but that path already contains a profile";
  }

  if (!file_util::PathExists(path)) {
    // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
    // profile directory.  We should eventually be able to run in this
    // situation.
    if (!file_util::CreateDirectory(path))
      return NULL;
  }

  return Profile::CreateProfile(path);
}
