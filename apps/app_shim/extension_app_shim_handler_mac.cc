// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/extension_app_shim_handler_mac.h"

#include "apps/app_shim/app_shim_messages.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "ui/base/cocoa/focus_window_set.h"

namespace apps {

ExtensionAppShimHandler::ExtensionAppShimHandler() {}

ExtensionAppShimHandler::~ExtensionAppShimHandler() {
  for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
    // Increment the iterator first as OnAppClosed may call back to OnShimClose
    // and invalidate the iterator.
    it++->second->OnAppClosed();
  }
}

bool ExtensionAppShimHandler::OnShimLaunch(Host* host) {
  Profile* profile = host->GetProfile();
  DCHECK(profile);

  const std::string& app_id = host->GetAppId();
  if (!extensions::Extension::IdIsValid(app_id)) {
    LOG(ERROR) << "Bad app ID from app shim launch message.";
    return false;
  }

  if (!LaunchApp(profile, app_id))
    return false;

  // The first host to claim this (profile, app_id) becomes the main host.
  // For any others, we launch the app but return false.
  if (!hosts_.insert(make_pair(make_pair(profile, app_id), host)).second)
    return false;

  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(profile))) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                   content::Source<Profile>(profile));
  }

  return true;
}

void ExtensionAppShimHandler::OnShimClose(Host* host) {
  HostMap::iterator it = hosts_.find(make_pair(host->GetProfile(),
                                               host->GetAppId()));
  // Any hosts other than the main host will still call OnShimClose, so ignore
  // them.
  if (it != hosts_.end() && it->second == host)
    hosts_.erase(it);
}

void ExtensionAppShimHandler::OnShimFocus(Host* host) {
  if (!host->GetProfile())
    return;

  extensions::ShellWindowRegistry* registry =
      extensions::ShellWindowRegistry::Get(host->GetProfile());
  const extensions::ShellWindowRegistry::ShellWindowList windows =
      registry->GetShellWindowsForApp(host->GetAppId());
  std::set<gfx::NativeWindow> native_windows;
  for (extensions::ShellWindowRegistry::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    native_windows.insert((*it)->GetNativeWindow());
  }
  ui::FocusWindowSet(native_windows);
}

void ExtensionAppShimHandler::OnShimQuit(Host* host) {
  if (!host->GetProfile())
    return;

  extensions::ShellWindowRegistry::ShellWindowList windows =
      extensions::ShellWindowRegistry::Get(host->GetProfile())->
          GetShellWindowsForApp(host->GetAppId());
  for (extensions::ShellWindowRegistry::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    (*it)->GetBaseWindow()->Close();
  }
}

bool ExtensionAppShimHandler::LaunchApp(Profile* profile,
                                        const std::string& app_id) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionServiceInterface* extension_service =
      extension_system->extension_service();
  const extensions::Extension* extension =
      extension_service->GetExtensionById(app_id, false);
  if (!extension) {
    LOG(ERROR) << "Attempted to launch nonexistent app with id '"
               << app_id << "'.";
    return false;
  }
  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
  return true;
}

void ExtensionAppShimHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = content::Source<Profile>(source).ptr();
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      extensions::ExtensionHost* extension_host =
          content::Details<extensions::ExtensionHost>(details).ptr();
      CloseShim(profile, extension_host->extension_id());
      break;
    }
    default:
      NOTREACHED();  // Unexpected notification.
      break;
  }
}

void ExtensionAppShimHandler::CloseShim(Profile* profile,
                                        const std::string& app_id) {
  HostMap::const_iterator it = hosts_.find(make_pair(profile, app_id));
  if (it != hosts_.end())
    it->second->OnAppClosed();
}

}  // namespace apps
