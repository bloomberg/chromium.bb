// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_manager_mac.h"

#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_shim/app_shim_host_mac.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "ipc/unix_domain_socket_util.h"

using content::BrowserThread;

namespace {

void CreateAppShimHost(const IPC::ChannelHandle& handle) {
  // AppShimHost takes ownership of itself.
  (new AppShimHost)->ServeChannel(handle);
}

}  // namespace

const base::FilePath* AppShimHostManager::g_override_user_data_dir_ = NULL;

AppShimHostManager::AppShimHostManager() {}

void AppShimHostManager::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  apps::AppShimHandler::SetDefaultHandler(&extension_app_shim_handler_);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AppShimHostManager::InitOnFileThread, this));
}

AppShimHostManager::~AppShimHostManager() {
  apps::AppShimHandler::SetDefaultHandler(NULL);
}

void AppShimHostManager::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath user_data_dir;
  if (g_override_user_data_dir_) {
    user_data_dir = *g_override_user_data_dir_;
  } else if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Couldn't get user data directory while creating App Shim "
               << "Host manager.";
    return;
  }

  base::FilePath socket_path =
      user_data_dir.Append(app_mode::kAppShimSocketName);
  // This mirrors a check in unix_domain_socket_util.cc which will guarantee
  // failure and spam log files on bots because they have deeply nested paths to
  // |user_data_dir| when swarming. See http://crbug.com/240554. Shim tests that
  // run on the bots must override the path using AppShimHostManagerTestApi.
  if (socket_path.value().length() >= IPC::kMaxSocketNameLength &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return;
  }

  factory_.reset(new IPC::ChannelFactory(socket_path, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppShimHostManager::ListenOnIOThread, this));
}

void AppShimHostManager::ListenOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!factory_->Listen()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AppShimHostManager::OnListenError, this));
  }
}

void AppShimHostManager::OnClientConnected(
    const IPC::ChannelHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateAppShimHost, handle));
}

void AppShimHostManager::OnListenError() {
  // TODO(tapted): Set a timeout and attempt to reconstruct the channel. Until
  // cases where the error could occur are better known, just reset the factory
  // to allow failure to be communicated via the test API.
  factory_.reset();
}
