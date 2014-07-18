// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_manager_mac.h"

#include <unistd.h>

#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_shim/app_shim_host_mac.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"

using content::BrowserThread;

namespace {

void CreateAppShimHost(const IPC::ChannelHandle& handle) {
  // AppShimHost takes ownership of itself.
  (new AppShimHost)->ServeChannel(handle);
}

base::FilePath GetDirectoryInTmpTemplate(const base::FilePath& user_data_dir) {
  base::FilePath temp_dir;
  CHECK(PathService::Get(base::DIR_TEMP, &temp_dir));
  // Check that it's shorter than the IPC socket length (104) minus the
  // intermediate folder ("/chrome-XXXXXX/") and kAppShimSocketShortName.
  DCHECK_GT(83u, temp_dir.value().length());
  return temp_dir.Append("chrome-XXXXXX");
}

void DeleteSocketFiles(const base::FilePath& directory_in_tmp,
                       const base::FilePath& symlink_path) {
  if (!directory_in_tmp.empty())
    base::DeleteFile(directory_in_tmp, true);
  if (!symlink_path.empty())
    base::DeleteFile(symlink_path, false);
}

}  // namespace

AppShimHostManager::AppShimHostManager()
    : did_init_(false) {}

void AppShimHostManager::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!did_init_);
  did_init_ = true;
  apps::AppShimHandler::SetDefaultHandler(&extension_app_shim_handler_);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AppShimHostManager::InitOnFileThread, this));
}

AppShimHostManager::~AppShimHostManager() {
  acceptor_.reset();
  if (!did_init_)
    return;

  apps::AppShimHandler::SetDefaultHandler(NULL);
  base::FilePath symlink_path;
  if (PathService::Get(chrome::DIR_USER_DATA, &symlink_path))
    symlink_path = symlink_path.Append(app_mode::kAppShimSocketSymlinkName);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&DeleteSocketFiles, directory_in_tmp_, symlink_path));
}

void AppShimHostManager::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return;

  // The socket path must be shorter than 104 chars (IPC::kMaxSocketNameLength).
  // To accommodate this, we use a short path in /tmp/ that is generated from a
  // hash of the user data dir.
  std::string directory_string =
      GetDirectoryInTmpTemplate(user_data_dir).value();

  // mkdtemp() replaces trailing X's randomly and creates the directory.
  if (!mkdtemp(&directory_string[0])) {
    PLOG(ERROR) << directory_string;
    return;
  }

  directory_in_tmp_ = base::FilePath(directory_string);
  // Check that the directory was created with the correct permissions.
  int dir_mode = 0;
  if (!base::GetPosixFilePermissions(directory_in_tmp_, &dir_mode) ||
      dir_mode != base::FILE_PERMISSION_USER_MASK) {
    NOTREACHED();
    return;
  }

  // UnixDomainSocketAcceptor creates the socket immediately.
  base::FilePath socket_path =
      directory_in_tmp_.Append(app_mode::kAppShimSocketShortName);
  acceptor_.reset(new apps::UnixDomainSocketAcceptor(socket_path, this));

  // Create a symlink to the socket in the user data dir. This lets the shim
  // process started from Finder find the actual socket path by following the
  // symlink with ::readlink().
  base::FilePath symlink_path =
      user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);
  base::DeleteFile(symlink_path, false);
  base::CreateSymbolicLink(socket_path, symlink_path);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppShimHostManager::ListenOnIOThread, this));
}

void AppShimHostManager::ListenOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!acceptor_->Listen()) {
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
  // cases where the error could occur are better known, just reset the acceptor
  // to allow failure to be communicated via the test API.
  acceptor_.reset();
}
