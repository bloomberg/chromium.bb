// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/user_policy_disk_cache.h"
#include "content/browser/browser_thread.h"

namespace {

// Other places can sample on the same UMA counter, so make sure they all do
// it on the same thread (UI).
void SampleUMAOnUIThread(policy::MetricPolicy sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy, sample,
                            policy::kMetricPolicySize);
}

void SampleUMA(policy::MetricPolicy sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableFunction(&SampleUMAOnUIThread, sample));
}

}  // namespace

namespace policy {

UserPolicyDiskCache::Delegate::~Delegate() {}

UserPolicyDiskCache::UserPolicyDiskCache(
    const base::WeakPtr<Delegate>& delegate,
    const FilePath& backing_file_path)
    : delegate_(delegate),
      backing_file_path_(backing_file_path) {}

void UserPolicyDiskCache::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &UserPolicyDiskCache::LoadOnFileThread));
}

void UserPolicyDiskCache::Store(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &UserPolicyDiskCache::StoreOnFileThread, policy));
}

UserPolicyDiskCache::~UserPolicyDiskCache() {}

void UserPolicyDiskCache::LoadOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::PathExists(backing_file_path_))
    return;

  // Read the protobuf from the file.
  std::string data;
  if (!file_util::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    SampleUMA(kMetricPolicyLoadFailed);
    return;
  }

  // Decode it.
  em::CachedCloudPolicyResponse cached_response;
  if (!cached_response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    SampleUMA(kMetricPolicyLoadFailed);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &UserPolicyDiskCache::FinishLoadOnUIThread,
                        cached_response));
}

void UserPolicyDiskCache::FinishLoadOnUIThread(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadSucceeded,
                            kMetricPolicySize);
  if (delegate_.get())
    delegate_->OnDiskCacheLoaded(policy);
}

void UserPolicyDiskCache::StoreOnFileThread(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string data;
  if (!policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    SampleUMA(kMetricPolicyStoreFailed);
    return;
  }

  if (!file_util::CreateDirectory(backing_file_path_.DirName())) {
    LOG(WARNING) << "Failed to create directory "
                 << backing_file_path_.DirName().value();
    SampleUMA(kMetricPolicyStoreFailed);
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(backing_file_path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << backing_file_path_.value();
    SampleUMA(kMetricPolicyStoreFailed);
    return;
  }
  SampleUMA(kMetricPolicyStoreSucceeded);
}

}  // namespace policy
