// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/login/mount_manager.h"

#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {

const char kBaseChromeosMountPath[] = "/home/user";

MountManager* MountManager::Get() {
  if (!instance_)
    instance_ = new MountManager();
  return instance_;
}

// static
MountManager* MountManager::instance_ = NULL;

base::FilePath MountManager::GetHomeDir(std::string& user_hash) {
  return base::FilePath(kBaseChromeosMountPath).Append(user_hash);
}

MountManager::MountManager() {}

MountManager::~MountManager() {}

bool MountManager::IsMounted(const std::string& user_id) {
  UserToPathMap::iterator i(additional_mounts_.find(user_id));
  return i != additional_mounts_.end();
}

base::FilePath MountManager::GetPath(const std::string& user_id) {
  UserToPathMap::iterator i(additional_mounts_.find(user_id));
  DCHECK(i != additional_mounts_.end());
  return (i == additional_mounts_.end()) ? base::FilePath() : i->second;
}

void MountManager::SetPath(const std::string& user_id,
                           const base::FilePath& path) {
  additional_mounts_[user_id] = path;
}

void MountManager::DeletePath(const std::string& user_id) {
  additional_mounts_.erase(user_id);
}

}  // namespace chromeos
