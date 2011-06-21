// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_token_cache.h"

#include "base/file_util.h"
#include "content/browser/browser_thread.h"

namespace policy {

namespace em = enterprise_management;

UserPolicyTokenCache::Delegate::~Delegate() {}

UserPolicyTokenCache::UserPolicyTokenCache(
    const base::WeakPtr<Delegate>& delegate,
    const FilePath& cache_file)
    : delegate_(delegate),
      cache_file_(cache_file) {}

void UserPolicyTokenCache::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &UserPolicyTokenCache::LoadOnFileThread));
}

void UserPolicyTokenCache::Store(const std::string& token,
                                 const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &UserPolicyTokenCache::StoreOnFileThread,
                        token,
                        device_id));
}

UserPolicyTokenCache::~UserPolicyTokenCache() {
}

void UserPolicyTokenCache::LoadOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string device_token;
  std::string device_id;

  if (file_util::PathExists(cache_file_)) {
    std::string data;
    em::DeviceCredentials device_credentials;
    if (file_util::ReadFileToString(cache_file_, &data) &&
        device_credentials.ParseFromArray(data.c_str(), data.size())) {
      device_token = device_credentials.device_token();
      device_id = device_credentials.device_id();
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &UserPolicyTokenCache::NotifyOnUIThread,
                        device_token,
                        device_id));
}

void UserPolicyTokenCache::NotifyOnUIThread(const std::string& token,
                                            const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_.get())
    delegate_->OnTokenCacheLoaded(token, device_id);
}

void UserPolicyTokenCache::StoreOnFileThread(const std::string& token,
                                             const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  em::DeviceCredentials device_credentials;
  device_credentials.set_device_token(token);
  device_credentials.set_device_id(device_id);
  std::string data;
  bool success = device_credentials.SerializeToString(&data);
  if (!success) {
    LOG(WARNING) << "Failed serialize device token data, will not write "
                 << cache_file_.value();
    return;
  }

  if (!file_util::CreateDirectory(cache_file_.DirName())) {
    LOG(WARNING) << "Failed to create directory "
                 << cache_file_.DirName().value();
    return;
  }

  file_util::WriteFile(cache_file_, data.c_str(), data.length());
}

}  // namespace policy
