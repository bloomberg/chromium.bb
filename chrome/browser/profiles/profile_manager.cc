// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
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


bool ProfileManagerObserver::DeleteAfterCreation() {
    return false;
}

// The NewProfileLauncher class is created when to wait for a multi-profile
// to be created asynchronously. Upon completion of profile creation, the
// NPL takes care of launching a new browser window and signing the user
// in to their Google account.
class NewProfileLauncher : public ProfileManagerObserver {
 public:
  virtual void OnProfileCreated(Profile* profile) {
    Browser::NewWindowWithProfile(profile);
    ProfileSyncService* service = profile->GetProfileSyncService();
    DCHECK(service);
    service->ShowLoginDialog();
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_PROFILE_MENU);
  }

  virtual bool DeleteAfterCreation() OVERRIDE { return true; }
};

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    SessionServiceFactory::ShutdownForProfile(profiles[i]);
}

// static
Profile* ProfileManager::GetDefaultProfile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

ProfileManager::ProfileManager() : logged_in_(false) {
  base::SystemMonitor::Get()->AddObserver(this);
  BrowserList::AddObserver(this);
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      NotificationType::LOGIN_USER_CHANGED,
      NotificationService::AllSources());
#endif
}

ProfileManager::~ProfileManager() {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
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
  // TODO(mirandac): should not automatically be default profile.
  relative_profile_dir =
      relative_profile_dir.AppendASCII(chrome::kNotSignedInProfile);
  return relative_profile_dir;
}

Profile* ProfileManager::GetLastUsedProfile(const FilePath& user_data_dir) {
  FilePath last_used_profile_dir(user_data_dir);
  std::string last_profile_used;
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (local_state->HasPrefPath(prefs::kProfileLastUsed))
    last_profile_used = local_state->GetString(prefs::kProfileLastUsed);
  last_used_profile_dir = last_profile_used.empty() ?
      last_used_profile_dir.AppendASCII(chrome::kNotSignedInProfile) :
      last_used_profile_dir.AppendASCII(last_profile_used);
  return GetProfile(last_used_profile_dir);
}

void ProfileManager::RegisterProfileName(Profile* profile) {
  std::string profile_name = profile->GetProfileName();
  std::string dir_base = profile->GetPath().BaseName().MaybeAsASCII();
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs::kProfileDirectoryMap);
  DictionaryValue* path_map = update.Get();
  // We don't check for duplicates because we should be able to overwrite
  // path->name mappings here, if the user syncs a local account to a
  // different Google account.
  path_map->SetString(dir_base, profile_name);
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
                                        ProfileManagerObserver* observer) {
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
void ProfileManager::CreateDefaultProfileAsync(
    ProfileManagerObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  FilePath default_profile_dir;
  PathService::Get(chrome::DIR_USER_DATA, &default_profile_dir);
  // TODO(mirandac): current directory will not always be default in the future
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

void ProfileManager::OnBrowserAdded(const Browser* browser) {}

void ProfileManager::OnBrowserRemoved(const Browser* browser) {}

void ProfileManager::OnBrowserSetLastActive(const Browser* browser) {
  Profile* last_active = browser->GetProfile();
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetString(prefs::kProfileLastUsed,
      last_active->GetPath().BaseName().MaybeAsASCII());
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

  std::vector<ProfileManagerObserver*> observers;
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

  std::vector<ProfileManagerObserver*> observers_to_delete;

  for (size_t i = 0; i < observers.size(); ++i) {
    observers[i]->OnProfileCreated(profile);
    if (observers[i]->DeleteAfterCreation())
      observers_to_delete.push_back(observers[i]);
  }

  observers_to_delete.clear();
}

// static
void ProfileManager::CreateMultiProfileAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Create the next profile in the next available directory slot.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::IntToString(next_directory));
  FilePath new_path;
  PathService::Get(chrome::DIR_USER_DATA, &new_path);
#if defined(OS_WIN)
  new_path = new_path.Append(ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The launcher is deleted by the manager when profile creation is finished.
  NewProfileLauncher* launcher = new NewProfileLauncher();
  profile_manager->CreateProfileAsync(new_path, launcher);
}

// static
void ProfileManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kProfileLastUsed, "");
  prefs->RegisterDictionaryPref(prefs::kProfileDirectoryMap);
  prefs->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
}
