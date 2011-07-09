// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_token_cache.h"

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"

namespace {

// Other places can sample on the same UMA counter, so make sure they all do
// it on the same thread (UI).
void SampleUMAOnUIThread(policy::MetricToken sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricToken, sample,
                            policy::kMetricTokenSize);
}

void SampleUMA(policy::MetricToken sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableFunction(&SampleUMAOnUIThread, sample));
}

}  // namespace

namespace policy {

namespace em = enterprise_management;

UserPolicyTokenCache::UserPolicyTokenCache(
    const base::WeakPtr<CloudPolicyDataStore>& data_store,
    const FilePath& cache_file)
    : data_store_(data_store),
      cache_file_(cache_file) {
  data_store_->AddObserver(this);
}

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
  if (data_store_)
    data_store_->RemoveObserver(this);
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
      SampleUMA(kMetricTokenLoadSucceeded);
    } else {
      SampleUMA(kMetricTokenLoadFailed);
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
  if (data_store_) {
    data_store_->set_device_id(device_id);
    data_store_->SetDeviceToken(token, true);
    data_store_->NotifyDeviceTokenChanged();
  }
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
    SampleUMA(kMetricTokenStoreFailed);
    return;
  }

  if (!file_util::CreateDirectory(cache_file_.DirName())) {
    LOG(WARNING) << "Failed to create directory "
                 << cache_file_.DirName().value();
    SampleUMA(kMetricTokenStoreFailed);
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(cache_file_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << cache_file_.value();
    SampleUMA(kMetricTokenStoreFailed);
  }

  SampleUMA(kMetricTokenStoreSucceeded);
}

void UserPolicyTokenCache::OnDeviceTokenChanged() {
  if (!data_store_->device_token().empty() &&
      !data_store_->device_id().empty()) {
    // This will also happen after loading the cache.
    Store(data_store_->device_token(), data_store_->device_id());
  }
}

void UserPolicyTokenCache::OnCredentialsChanged() {
}

void UserPolicyTokenCache::OnDataStoreGoingAway() {
  data_store_->RemoveObserver(this);
}

}  // namespace policy
