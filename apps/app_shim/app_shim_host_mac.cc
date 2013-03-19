// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_mac.h"

#include "apps/app_shim/app_shim_messages.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/base/cocoa/focus_window_set.h"

AppShimHost::AppShimHost()
    : channel_(NULL), profile_(NULL) {
}

AppShimHost::~AppShimHost() {
  DCHECK(CalledOnValidThread());
}

void AppShimHost::ServeChannel(const IPC::ChannelHandle& handle) {
  DCHECK(CalledOnValidThread());
  DCHECK(!channel_.get());
  channel_.reset(new IPC::ChannelProxy(handle, IPC::Channel::MODE_SERVER, this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
}

bool AppShimHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppShimHost, message)
    IPC_MESSAGE_HANDLER(AppShimHostMsg_LaunchApp, OnLaunchApp)
    IPC_MESSAGE_HANDLER(AppShimHostMsg_FocusApp, OnFocus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool AppShimHost::Send(IPC::Message* message) {
  DCHECK(channel_.get());
  return channel_->Send(message);
}

void AppShimHost::OnLaunchApp(std::string profile_dir, std::string app_id) {
  DCHECK(CalledOnValidThread());
  bool success = LaunchAppImpl(profile_dir, app_id);
  Send(new AppShimMsg_LaunchApp_Done(success));
}

void AppShimHost::OnFocus() {
  DCHECK(CalledOnValidThread());
  if (!profile_)
    return;
  extensions::ShellWindowRegistry* registry =
      extensions::ShellWindowRegistry::Get(profile_);
  const std::set<ShellWindow*> windows =
      registry->GetShellWindowsForApp(app_id_);
  std::set<gfx::NativeWindow> native_windows;
  for (std::set<ShellWindow*>::const_iterator i = windows.begin();
       i != windows.end();
       ++i) {
    native_windows.insert((*i)->GetNativeWindow());
  }
  ui::FocusWindowSet(native_windows);
}

bool AppShimHost::LaunchAppImpl(const std::string& profile_dir,
                                const std::string& app_id) {
  DCHECK(CalledOnValidThread());
  if (profile_) {
    // Only one app launch message per channel.
    return false;
  }
  if (!extensions::Extension::IdIsValid(app_id)) {
    LOG(ERROR) << "Bad app ID from app shim launch message.";
    return false;
  }
  Profile* profile = FetchProfileForDirectory(profile_dir);
  if (!profile)
    return false;

  if (!LaunchApp(profile, app_id))
    return false;

  profile_ = profile;
  app_id_ = app_id;

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<Profile>(profile_));
  return true;
}

Profile* AppShimHost::FetchProfileForDirectory(const std::string& profile_dir) {
  ProfileManager* profileManager = g_browser_process->profile_manager();
  // Even though the name of this function is "unsafe", there's no security
  // issue here -- the check for the profile name in the profile info cache
  // ensures that we never access any directory that isn't a known profile.
  base::FilePath path = base::FilePath::FromUTF8Unsafe(profile_dir);
  path = profileManager->user_data_dir().Append(path);
  ProfileInfoCache& cache = profileManager->GetProfileInfoCache();
  // This ensures that the given profile path is acutally a profile that we
  // already know about.
  if (cache.GetIndexOfProfileWithPath(path) == std::string::npos) {
    LOG(ERROR) << "Requested directory is not a known profile.";
    return NULL;
  }
  Profile* profile = profileManager->GetProfile(path);
  if (!profile) {
    LOG(ERROR) << "Couldn't get profile for directory '" << profile_dir << "'.";
    return NULL;
  }
  return profile;
}

bool AppShimHost::LaunchApp(Profile* profile, const std::string& app_id) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionServiceInterface* extension_service =
      extension_system->extension_service();
  const extensions::Extension* extension =
      extension_service->GetExtensionById(
          app_id, false);
  if (!extension) {
    LOG(ERROR) << "Attempted to launch nonexistent app with id '"
               << app_id << "'.";
    return false;
  }
  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  chrome::AppLaunchParams params(profile,
                                 extension,
                                 extension_misc::LAUNCH_NONE,
                                 NEW_WINDOW);
  chrome::OpenApplication(params);
  return true;
}

void AppShimHost::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  DCHECK(content::Source<Profile>(source).ptr() == profile_);
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      extensions::ExtensionHost* extension_host =
          content::Details<extensions::ExtensionHost>(details).ptr();
      if (app_id_ == extension_host->extension_id())
        Close();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void AppShimHost::Close() {
  DCHECK(CalledOnValidThread());
  delete this;
}
