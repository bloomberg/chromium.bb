// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_manager_mac.h"

#include "apps/app_shim/app_shim_host_mac.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/common/mac/app_mode_common.h"

using content::BrowserThread;

namespace {

void CreateAppShimHost(const IPC::ChannelHandle& handle) {
  // AppShimHost takes ownership of itself.
  (new AppShimHost)->ServeChannel(handle);
}

}  // namespace

AppShimHostManager::AppShimHostManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AppShimHostManager::InitOnFileThread,
          base::Unretained(this)));
}

AppShimHostManager::~AppShimHostManager() {
}

void AppShimHostManager::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Couldn't get user data directory while creating App Shim "
               << "Host manager.";
    return;
  }
  base::FilePath socket_path =
      user_data_dir.Append(app_mode::kAppShimSocketName);
  factory_.reset(new IPC::ChannelFactory(socket_path, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppShimHostManager::ListenOnIOThread,
          base::Unretained(this)));
}

void AppShimHostManager::ListenOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  factory_->Listen();
}

void AppShimHostManager::OnClientConnected(
    const IPC::ChannelHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateAppShimHost, handle));
}

void AppShimHostManager::OnListenError() {
  // TODO(jeremya): set a timeout and attempt to reconstruct the channel.
}
