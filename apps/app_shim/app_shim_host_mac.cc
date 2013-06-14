// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_mac.h"

#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_shim/app_shim_messages.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"

AppShimHost::AppShimHost() : profile_(NULL) {}

AppShimHost::~AppShimHost() {
  DCHECK(CalledOnValidThread());
  apps::AppShimHandler* handler = apps::AppShimHandler::GetForAppMode(app_id_);
  if (handler)
    handler->OnShimClose(this);
}

void AppShimHost::ServeChannel(const IPC::ChannelHandle& handle) {
  DCHECK(CalledOnValidThread());
  DCHECK(!channel_.get());
  channel_.reset(new IPC::ChannelProxy(
      handle,
      IPC::Channel::MODE_SERVER,
      this,
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO).get()));
}

Profile* AppShimHost::GetProfile() const {
  return profile_;
}

std::string AppShimHost::GetAppId() const {
  return app_id_;
}

bool AppShimHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppShimHost, message)
    IPC_MESSAGE_HANDLER(AppShimHostMsg_LaunchApp, OnLaunchApp)
    IPC_MESSAGE_HANDLER(AppShimHostMsg_FocusApp, OnFocus)
    IPC_MESSAGE_HANDLER(AppShimHostMsg_QuitApp, OnQuit)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AppShimHost::OnChannelError() {
  Close();
}

bool AppShimHost::Send(IPC::Message* message) {
  DCHECK(channel_.get());
  return channel_->Send(message);
}

void AppShimHost::OnLaunchApp(base::FilePath profile_dir,
                              std::string app_id,
                              apps::AppShimLaunchType launch_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(!profile_);
  if (profile_) {
    // Only one app launch message per channel.
    Send(new AppShimMsg_LaunchApp_Done(false));
    return;
  }

  if (!(profile_ = FetchProfileForDirectory(profile_dir))) {
    Send(new AppShimMsg_LaunchApp_Done(false));
    return;
  }

  app_id_ = app_id;

  apps::AppShimHandler* handler = apps::AppShimHandler::GetForAppMode(app_id_);
  bool success = handler && handler->OnShimLaunch(this, launch_type);
  Send(new AppShimMsg_LaunchApp_Done(success));
}

void AppShimHost::OnFocus() {
  DCHECK(CalledOnValidThread());
  apps::AppShimHandler* handler = apps::AppShimHandler::GetForAppMode(app_id_);
  if (handler)
    handler->OnShimFocus(this);
}

void AppShimHost::OnQuit() {
  DCHECK(CalledOnValidThread());
  apps::AppShimHandler* handler = apps::AppShimHandler::GetForAppMode(app_id_);
  if (handler)
    handler->OnShimQuit(this);
}

Profile* AppShimHost::FetchProfileForDirectory(
    const base::FilePath& profile_dir) {
  ProfileManager* profileManager = g_browser_process->profile_manager();
  // Check for the profile name in the profile info cache to ensure that we
  // never access any directory that isn't a known profile.
  base::FilePath path = profileManager->user_data_dir().Append(profile_dir);
  ProfileInfoCache& cache = profileManager->GetProfileInfoCache();
  if (cache.GetIndexOfProfileWithPath(path) == std::string::npos) {
    LOG(ERROR) << "Requested directory is not a known profile '"
               << profile_dir.value() << "'.";
    return NULL;
  }
  Profile* profile = profileManager->GetProfile(path);
  if (!profile) {
    LOG(ERROR) << "Couldn't get profile for directory '"
               << profile_dir.value() << "'.";
    return NULL;
  }
  return profile;
}

void AppShimHost::OnAppClosed() {
  Close();
}

void AppShimHost::Close() {
  DCHECK(CalledOnValidThread());
  delete this;
}
