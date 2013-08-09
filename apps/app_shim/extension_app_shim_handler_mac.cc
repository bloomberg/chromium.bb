// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/extension_app_shim_handler_mac.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_shim/app_shim_host_manager_mac.h"
#include "apps/app_shim/app_shim_messages.h"
#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/cocoa/focus_window_set.h"

namespace {

void ProfileLoadedCallback(base::Callback<void(Profile*)> callback,
                           Profile* profile,
                           Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    callback.Run(profile);
    return;
  }

  // This should never get an error since it only loads existing profiles.
  DCHECK_EQ(Profile::CREATE_STATUS_CREATED, status);
}

void TerminateIfNoShellWindows() {
  bool shell_windows_left =
      apps::ShellWindowRegistry::IsShellWindowRegisteredInAnyProfile(0);
  if (!shell_windows_left)
    chrome::AttemptExit();
}

}  // namespace

namespace apps {

typedef ShellWindowRegistry::ShellWindowList ShellWindowList;

bool ExtensionAppShimHandler::Delegate::ProfileExistsForPath(
    const base::FilePath& path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Check for the profile name in the profile info cache to ensure that we
  // never access any directory that isn't a known profile.
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  return cache.GetIndexOfProfileWithPath(full_path) != std::string::npos;
}

Profile* ExtensionAppShimHandler::Delegate::ProfileForPath(
    const base::FilePath& path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  Profile* profile = profile_manager->GetProfileByPath(full_path);

  // Use IsValidProfile to check if the profile has been created.
  return profile && profile_manager->IsValidProfile(profile) ? profile : NULL;
}

void ExtensionAppShimHandler::Delegate::LoadProfileAsync(
    const base::FilePath& path,
    base::Callback<void(Profile*)> callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  profile_manager->CreateProfileAsync(
      full_path,
      base::Bind(&ProfileLoadedCallback, callback),
      string16(), string16(), std::string());
}

ShellWindowList ExtensionAppShimHandler::Delegate::GetWindows(
    Profile* profile,
    const std::string& extension_id) {
  return ShellWindowRegistry::Get(profile)->GetShellWindowsForApp(extension_id);
}

const extensions::Extension*
ExtensionAppShimHandler::Delegate::GetAppExtension(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);
  const extensions::Extension* extension =
      extension_service->GetExtensionById(extension_id, false);
  return extension && extension->is_platform_app() ? extension : NULL;
}

void ExtensionAppShimHandler::Delegate::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension) {
  chrome::OpenApplication(
      chrome::AppLaunchParams(profile, extension, NEW_FOREGROUND_TAB));
}

void ExtensionAppShimHandler::Delegate::LaunchShim(
    Profile* profile,
    const extensions::Extension* extension) {
  web_app::MaybeLaunchShortcut(
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile));
}

void ExtensionAppShimHandler::Delegate::MaybeTerminate() {
  // Post this to give ShellWindows a chance to remove themselves from the
  // registry.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TerminateIfNoShellWindows));
}

ExtensionAppShimHandler::ExtensionAppShimHandler()
    : delegate_(new Delegate),
      browser_opened_ever_(false),
      weak_factory_(this) {
  // This is instantiated in BrowserProcessImpl::PreMainMessageLoopRun with
  // AppShimHostManager. Since PROFILE_CREATED is not fired until
  // ProfileManager::GetLastUsedProfile/GetLastOpenedProfiles, this should catch
  // notifications for all profiles.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ExtensionAppShimHandler::~ExtensionAppShimHandler() {}

AppShimHandler::Host* ExtensionAppShimHandler::FindHost(
    Profile* profile,
    const std::string& app_id) {
  HostMap::iterator it = hosts_.find(make_pair(profile, app_id));
  return it == hosts_.end() ? NULL : it->second;
}

// static
void ExtensionAppShimHandler::QuitAppForWindow(ShellWindow* shell_window) {
  ExtensionAppShimHandler* handler =
      g_browser_process->platform_part()->app_shim_host_manager()->
          extension_app_shim_handler();
  Host* host = handler->FindHost(shell_window->profile(),
                                 shell_window->extension_id());
  if (host) {
    handler->OnShimQuit(host);
  } else {
    // App shims might be disabled or the shim is still starting up.
    ShellWindowRegistry::Get(shell_window->profile())->
        CloseAllShellWindowsForApp(shell_window->extension_id());
  }
}

void ExtensionAppShimHandler::OnShimLaunch(Host* host,
                                           AppShimLaunchType launch_type) {
  const std::string& app_id = host->GetAppId();
  DCHECK(extensions::Extension::IdIsValid(app_id));

  const base::FilePath& profile_path = host->GetProfilePath();
  DCHECK(!profile_path.empty());

  if (!delegate_->ProfileExistsForPath(profile_path)) {
    // User may have deleted the profile this shim was originally created for.
    // TODO(jackhou): Add some UI for this case and remove the LOG.
    LOG(ERROR) << "Requested directory is not a known profile '"
               << profile_path.value() << "'.";
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND);
    return;
  }

  Profile* profile = delegate_->ProfileForPath(profile_path);

  if (profile) {
    OnProfileLoaded(host, launch_type, profile);
    return;
  }

  // If the profile is not loaded, this must have been a launch by the shim.
  // Load the profile asynchronously, the host will be registered in
  // OnProfileLoaded.
  DCHECK_EQ(APP_SHIM_LAUNCH_NORMAL, launch_type);
  delegate_->LoadProfileAsync(
      profile_path,
      base::Bind(&ExtensionAppShimHandler::OnProfileLoaded,
                 weak_factory_.GetWeakPtr(),
                 host, launch_type));

  // Return now. OnAppLaunchComplete will be called when the app is activated.
}

void ExtensionAppShimHandler::OnProfileLoaded(Host* host,
                                              AppShimLaunchType launch_type,
                                              Profile* profile) {
  const std::string& app_id = host->GetAppId();
  // TODO(jackhou): Add some UI for this case and remove the LOG.
  const extensions::Extension* extension =
      delegate_->GetAppExtension(profile, app_id);
  if (!extension) {
    LOG(ERROR) << "Attempted to launch nonexistent app with id '"
               << app_id << "'.";
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_APP_NOT_FOUND);
    return;
  }

  // The first host to claim this (profile, app_id) becomes the main host.
  // For any others, focus or relaunch the app.
  if (!hosts_.insert(make_pair(make_pair(profile, app_id), host)).second) {
    OnShimFocus(host,
                launch_type == APP_SHIM_LAUNCH_NORMAL ?
                    APP_SHIM_FOCUS_REOPEN : APP_SHIM_FOCUS_NORMAL);
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_DUPLICATE_HOST);
    return;
  }

  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  if (launch_type == APP_SHIM_LAUNCH_NORMAL)
    delegate_->LaunchApp(profile, extension);
  else
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_SUCCESS);
}

void ExtensionAppShimHandler::OnShimClose(Host* host) {
  // This might be called when shutting down. Don't try to look up the profile
  // since profile_manager might not be around.
  for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
    HostMap::iterator current = it++;
    if (current->second == host)
      hosts_.erase(current);
  }
}

void ExtensionAppShimHandler::OnShimFocus(Host* host,
                                          AppShimFocusType focus_type) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const ShellWindowList windows =
      delegate_->GetWindows(profile, host->GetAppId());
  std::set<gfx::NativeWindow> native_windows;
  for (ShellWindowList::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    native_windows.insert((*it)->GetNativeWindow());
  }
  if (!native_windows.empty()) {
    ui::FocusWindowSet(native_windows);
    return;
  }

  if (focus_type == APP_SHIM_FOCUS_REOPEN) {
    const extensions::Extension* extension =
        delegate_->GetAppExtension(profile, host->GetAppId());
    if (extension) {
      delegate_->LaunchApp(profile, extension);
    } else {
      // Extensions may have been uninstalled or disabled since the shim
      // started.
      host->OnAppClosed();
    }
  }
}

void ExtensionAppShimHandler::OnShimSetHidden(Host* host, bool hidden) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const ShellWindowList windows =
      delegate_->GetWindows(profile, host->GetAppId());
  for (ShellWindowList::const_reverse_iterator it = windows.rbegin();
       it != windows.rend(); ++it) {
    if (hidden)
      (*it)->GetBaseWindow()->Hide();
    else
      (*it)->GetBaseWindow()->ShowInactive();
  }
}

void ExtensionAppShimHandler::OnShimQuit(Host* host) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const std::string& app_id = host->GetAppId();
  const ShellWindowList windows =
      delegate_->GetWindows(profile, app_id);
  for (ShellWindowRegistry::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    (*it)->GetBaseWindow()->Close();
  }

  DCHECK_NE(0u, hosts_.count(make_pair(profile, app_id)));
  host->OnAppClosed();

  if (!browser_opened_ever_ && hosts_.empty())
    delegate_->MaybeTerminate();
}

void ExtensionAppShimHandler::set_delegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void ExtensionAppShimHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
    registrar_.Remove(
        this, chrome::NOTIFICATION_BROWSER_OPENED,
        content::NotificationService::AllBrowserContextsAndSources());
    browser_opened_ever_ = true;
    return;
  }

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile->IsOffTheRecord())
    return;

  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      AppLifetimeMonitorFactory::GetForProfile(profile)->AddObserver(this);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      AppLifetimeMonitorFactory::GetForProfile(profile)->RemoveObserver(this);
      // Shut down every shim associated with this profile.
      for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
        // Increment the iterator first as OnAppClosed may call back to
        // OnShimClose and invalidate the iterator.
        HostMap::iterator current = it++;
        if (profile->IsSameProfile(current->first.first))
          current->second->OnAppClosed();
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      std::string app_id;
      if (type == chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED) {
        app_id = content::Details<extensions::ExtensionHost>(details).ptr()
            ->extension_id();
      } else {
        app_id = content::Details<extensions::Extension>(details).ptr()->id();
      }
      Host* host = FindHost(profile, app_id);
      if (host)
        host->OnAppClosed();
      break;
    }
    default: {
      NOTREACHED();  // Unexpected notification.
      break;
    }
  }
}

void ExtensionAppShimHandler::OnAppStart(Profile* profile,
                                         const std::string& app_id) {}

void ExtensionAppShimHandler::OnAppActivated(Profile* profile,
                                             const std::string& app_id) {
  const extensions::Extension* extension =
      delegate_->GetAppExtension(profile, app_id);
  if (!extension)
    return;

  Host* host = FindHost(profile, app_id);
  if (host) {
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_SUCCESS);
    OnShimFocus(host, APP_SHIM_FOCUS_NORMAL);
    return;
  }

  delegate_->LaunchShim(profile, extension);
}

void ExtensionAppShimHandler::OnAppDeactivated(Profile* profile,
                                               const std::string& app_id) {}

void ExtensionAppShimHandler::OnAppStop(Profile* profile,
                                        const std::string& app_id) {}

void ExtensionAppShimHandler::OnChromeTerminating() {}

}  // namespace apps
