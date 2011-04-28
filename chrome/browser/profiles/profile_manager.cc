// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#endif

namespace {

void SuspendURLRequestJobs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (net::URLRequestJobTracker::JobIterator i =
           net::g_url_request_job_tracker.begin();
       i != net::g_url_request_job_tracker.end(); ++i)
    (*i)->Kill();
}

void SuspendRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<net::URLRequestContext> request_context =
      request_context_getter->GetURLRequestContext();

  request_context->http_transaction_factory()->Suspend(true);
}

void ResumeRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<net::URLRequestContext> request_context =
      request_context_getter->GetURLRequestContext();
  request_context->http_transaction_factory()->Suspend(false);
}

}  // namespace

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    profiles[i]->ShutdownSessionService();
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
}

FilePath ProfileManager::GetDefaultProfileDir(
    const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kNotSignedInProfile);
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
  relative_profile_dir =
      relative_profile_dir.AppendASCII(chrome::kNotSignedInProfile);
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
      profile = GetProfile(default_profile_dir);
      profile = profile->GetOffTheRecordProfile();
    } else {
      profile = GetProfile(default_profile_dir);
    }
    return profile;
  }
#endif
  return GetProfile(default_profile_dir);
}

Profile* ProfileManager::GetProfileWithId(ProfileId profile_id) {
  DCHECK_NE(Profile::kInvalidProfileId, profile_id);
  for (ProfilesInfoMap::iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created) {
      Profile* candidate = iter->second->profile.get();
      if (candidate->GetRuntimeId() == profile_id)
        return candidate;
      if (candidate->HasOffTheRecordProfile()) {
        candidate = candidate->GetOffTheRecordProfile();
        if (candidate->GetRuntimeId() == profile_id)
          return candidate;
      }
    }
  }
  return NULL;
}

bool ProfileManager::IsValidProfile(Profile* profile) {
  for (ProfilesInfoMap::iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created) {
      Profile* candidate = iter->second->profile.get();
      if (candidate == profile ||
          (candidate->HasOffTheRecordProfile() &&
           candidate->GetOffTheRecordProfile() == profile)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (ProfilesInfoMap::const_iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created)
      profiles.push_back(iter->second->profile.get());
  }
  return profiles;
}

Profile* ProfileManager::GetProfile(const FilePath& profile_dir) {
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (NULL != profile)
    return profile;

  profile = Profile::CreateProfile(profile_dir);
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile);
    DCHECK(result);
  }
  return profile;
}

void ProfileManager::CreateProfileAsync(const FilePath& user_data_dir,
                                        Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfilesInfoMap::iterator iter = profiles_info_.find(user_data_dir);
  if (iter != profiles_info_.end()) {
    ProfileInfo* info = iter->second.get();
    if (info->created) {
      // Profile has already been created. Call observer immediately.
      observer->OnProfileCreated(info->profile.get());
    } else {
      // Profile is being created. Add observer to list.
      info->observers.push_back(observer);
    }
  } else {
    // Initiate asynchronous creation process.
    ProfileInfo* info =
        RegisterProfile(Profile::CreateProfileAsync(user_data_dir, this),
                        false);
    info->observers.push_back(observer);
  }
}

// static
void ProfileManager::CreateDefaultProfileAsync(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  FilePath default_profile_dir;
  PathService::Get(chrome::DIR_USER_DATA, &default_profile_dir);
  default_profile_dir = default_profile_dir.Append(
      profile_manager->GetCurrentProfileDir());

  profile_manager->CreateProfileAsync(default_profile_dir,
                                      observer);
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  RegisterProfile(profile, true);
  DoFinalInit(profile);
  return true;
}

ProfileManager::ProfileInfo* ProfileManager::RegisterProfile(Profile* profile,
                                                             bool created) {
  ProfileInfo* info = new ProfileInfo(profile, created);
  ProfilesInfoMap::iterator new_elem =
      (profiles_info_.insert(std::make_pair(profile->GetPath(), info))).first;
  return info;
}

Profile* ProfileManager::GetProfileByPath(const FilePath& path) const {
  ProfilesInfoMap::const_iterator iter = profiles_info_.find(path);
  return (iter == profiles_info_.end()) ? NULL : iter->second->profile.get();
}

void ProfileManager::OnSuspend() {
  DCHECK(CalledOnValidThread());

  bool posted = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&SuspendURLRequestJobs));
  DCHECK(posted);

  scoped_refptr<net::URLRequestContextGetter> request_context;
  std::vector<Profile*> profiles(GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    request_context = profiles[i]->GetRequestContext();
    posted = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&SuspendRequestContext, request_context));
    DCHECK(posted);
    request_context = profiles[i]->GetRequestContextForMedia();
    posted = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&SuspendRequestContext, request_context));
    DCHECK(posted);
  }
}

void ProfileManager::OnResume() {
  DCHECK(CalledOnValidThread());

  scoped_refptr<net::URLRequestContextGetter> request_context;
  std::vector<Profile*> profiles(GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    request_context = profiles[i]->GetRequestContext();
    bool posted = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&ResumeRequestContext, request_context));
    DCHECK(posted);
    request_context = profiles[i]->GetRequestContextForMedia();
    posted = BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&ResumeRequestContext, request_context));
    DCHECK(posted);
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

void ProfileManager::DoFinalInit(Profile* profile) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool init_extensions = true;
#if defined(OS_CHROMEOS)
  if (!logged_in_ &&
      (!command_line.HasSwitch(switches::kTestType) ||
        command_line.HasSwitch(switches::kLoginProfile))) {
    init_extensions = false;
  }
#endif
  profile->InitExtensions(init_extensions);

  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitPromoResources();
}

void ProfileManager::OnProfileCreated(Profile* profile, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<Observer*> observers;
  info->observers.swap(observers);

  if (success) {
    DoFinalInit(profile);
    info->created = true;
#if defined(OS_CHROMEOS)
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!logged_in_ &&
        (!command_line.HasSwitch(switches::kTestType) ||
         command_line.HasSwitch(switches::kLoginProfile))) {
      profile = profile->GetOffTheRecordProfile();
    }
#endif
  } else {
    profile = NULL;
    profiles_info_.erase(iter);
  }

  for (size_t i = 0; i < observers.size(); ++i) {
    observers[i]->OnProfileCreated(profile);
  }
}
