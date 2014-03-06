// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_policy_disk_cache.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "policy/proto/device_management_local.pb.h"

namespace em = enterprise_management;

namespace policy {

UserPolicyDiskCache::Delegate::~Delegate() {}

UserPolicyDiskCache::UserPolicyDiskCache(
    const base::WeakPtr<Delegate>& delegate,
    const base::FilePath& backing_file_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : delegate_(delegate),
      backing_file_path_(backing_file_path),
      origin_task_runner_(base::MessageLoopProxy::current()),
      background_task_runner_(background_task_runner) {}

void UserPolicyDiskCache::Load() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  bool ret = background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UserPolicyDiskCache::LoadOnFileThread, this));
  DCHECK(ret);
}

void UserPolicyDiskCache::Store(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserPolicyDiskCache::StoreOnFileThread, this, policy));
}

UserPolicyDiskCache::~UserPolicyDiskCache() {}

void UserPolicyDiskCache::LoadOnFileThread() {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  em::CachedCloudPolicyResponse cached_response;
  if (!base::PathExists(backing_file_path_)) {
    LoadDone(LOAD_RESULT_NOT_FOUND, cached_response);
    return;
  }

  // Read the protobuf from the file.
  std::string data;
  if (!base::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    LoadDone(LOAD_RESULT_READ_ERROR, cached_response);
    return;
  }

  // Decode it.
  if (!cached_response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    LoadDone(LOAD_RESULT_PARSE_ERROR, cached_response);
    return;
  }

  LoadDone(LOAD_RESULT_SUCCESS, cached_response);
}

void UserPolicyDiskCache::LoadDone(
    LoadResult result,
    const em::CachedCloudPolicyResponse& policy) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &UserPolicyDiskCache::ReportResultOnUIThread, this, result, policy));
}

void UserPolicyDiskCache::ReportResultOnUIThread(
    LoadResult result,
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());

  switch (result) {
    case LOAD_RESULT_NOT_FOUND:
      break;
    case LOAD_RESULT_READ_ERROR:
    case LOAD_RESULT_PARSE_ERROR:
      UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                                kMetricPolicyLoadFailed,
                                policy::kMetricPolicySize);
      break;
    case LOAD_RESULT_SUCCESS:
      UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                                kMetricPolicyLoadSucceeded,
                                policy::kMetricPolicySize);
      break;
  }

  if (delegate_.get())
    delegate_->OnDiskCacheLoaded(result, policy);
}

void UserPolicyDiskCache::StoreOnFileThread(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());
  std::string data;
  if (!policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                              kMetricPolicyStoreFailed,
                              policy::kMetricPolicySize);
    return;
  }

  if (!base::CreateDirectory(backing_file_path_.DirName())) {
    LOG(WARNING) << "Failed to create directory "
                 << backing_file_path_.DirName().value();
    UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                              kMetricPolicyStoreFailed,
                              policy::kMetricPolicySize);
    return;
  }

  int size = data.size();
  if (base::WriteFile(backing_file_path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << backing_file_path_.value();
    UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                              kMetricPolicyStoreFailed,
                              policy::kMetricPolicySize);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricPolicy,
                            kMetricPolicyStoreSucceeded,
                            policy::kMetricPolicySize);
}

}  // namespace policy
