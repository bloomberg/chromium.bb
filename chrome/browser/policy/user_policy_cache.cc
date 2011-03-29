// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_cache.h"

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"
#include "policy/configuration_policy_type.h"

namespace policy {

// Decodes a CloudPolicySettings object into two maps with mandatory and
// recommended settings, respectively. The implementation is generated code
// in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  PolicyMap* mandatory, PolicyMap* recommended);

// Saves policy information to a file.
class PersistPolicyTask : public Task {
 public:
  PersistPolicyTask(const FilePath& path,
                    const em::PolicyFetchResponse* cloud_policy_response,
                    const bool is_unmanaged)
      : path_(path),
        cloud_policy_response_(cloud_policy_response),
        is_unmanaged_(is_unmanaged) {}

 private:
  // Task override.
  virtual void Run();

  const FilePath path_;
  scoped_ptr<const em::PolicyFetchResponse> cloud_policy_response_;
  const bool is_unmanaged_;
};

void PersistPolicyTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string data;
  em::CachedCloudPolicyResponse cached_policy;
  if (cloud_policy_response_.get()) {
    cached_policy.mutable_cloud_policy()->CopyFrom(*cloud_policy_response_);
  }
  if (is_unmanaged_) {
    cached_policy.set_unmanaged(true);
    cached_policy.set_timestamp(base::Time::NowFromSystemTime().ToTimeT());
  }
  if (!cached_policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << path_.value();
    return;
  }
}

UserPolicyCache::UserPolicyCache(const FilePath& backing_file_path)
    : backing_file_path_(backing_file_path) {
}

UserPolicyCache::~UserPolicyCache() {
}

void UserPolicyCache::Load() {
  // TODO(jkummerow): This method is doing file IO during browser startup. In
  // the long run it would be better to delay this until the FILE thread exists.
  if (!file_util::PathExists(backing_file_path_) || initialization_complete()) {
    return;
  }

  // Read the protobuf from the file.
  std::string data;
  if (!file_util::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    return;
  }

  em::CachedCloudPolicyResponse cached_response;
  if (!cached_response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    return;
  }

  if (cached_response.unmanaged()) {
    SetUnmanagedInternal(base::Time::FromTimeT(cached_response.timestamp()));
  } else if (cached_response.has_cloud_policy()) {
    base::Time timestamp;
    if (SetPolicyInternal(cached_response.cloud_policy(), &timestamp, true))
      set_last_policy_refresh_time(timestamp);
  }
}

void UserPolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  base::Time now = base::Time::NowFromSystemTime();
  set_last_policy_refresh_time(now);
  bool ok = SetPolicyInternal(policy, NULL, false);
  if (ok)
    PersistPolicy(policy, now);
}

void UserPolicyCache::SetUnmanaged() {
  DCHECK(CalledOnValidThread());
  SetUnmanagedInternal(base::Time::NowFromSystemTime());
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      new PersistPolicyTask(backing_file_path_, NULL, true));
}

void UserPolicyCache::PersistPolicy(const em::PolicyFetchResponse& policy,
                                    const base::Time& timestamp) {
  if (timestamp > base::Time::NowFromSystemTime() +
                  base::TimeDelta::FromMinutes(1)) {
    LOG(WARNING) << "Server returned policy with timestamp from the future, "
                    "not persisting to disk.";
  } else {
    em::PolicyFetchResponse* policy_copy = new em::PolicyFetchResponse;
    policy_copy->CopyFrom(policy);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        new PersistPolicyTask(backing_file_path_, policy_copy, false));
  }
}

bool UserPolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                       PolicyMap* mandatory,
                                       PolicyMap* recommended) {
  // TODO(jkummerow): Verify policy_data.device_token(). Needs final
  // specification which token we're actually sending / expecting to get back.
  em::CloudPolicySettings policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse CloudPolicySettings protobuf.";
    return false;
  }
  DecodePolicy(policy, mandatory, recommended);
  return true;
}

}  // namespace policy
