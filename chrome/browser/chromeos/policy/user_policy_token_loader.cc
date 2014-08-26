// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_policy_token_loader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "policy/proto/device_management_local.pb.h"

namespace policy {

namespace em = enterprise_management;

UserPolicyTokenLoader::Delegate::~Delegate() {}

UserPolicyTokenLoader::UserPolicyTokenLoader(
    const base::WeakPtr<Delegate>& delegate,
    const base::FilePath& cache_file,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : delegate_(delegate),
      cache_file_(cache_file),
      origin_task_runner_(base::MessageLoopProxy::current()),
      background_task_runner_(background_task_runner) {}

void UserPolicyTokenLoader::Load() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserPolicyTokenLoader::LoadOnBackgroundThread, this));
}

UserPolicyTokenLoader::~UserPolicyTokenLoader() {
}

void UserPolicyTokenLoader::LoadOnBackgroundThread() {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());
  std::string device_token;
  std::string device_id;

  if (base::PathExists(cache_file_)) {
    std::string data;
    em::DeviceCredentials device_credentials;
    if (base::ReadFileToString(cache_file_, &data) &&
        device_credentials.ParseFromArray(data.c_str(), data.size())) {
      device_token = device_credentials.device_token();
      device_id = device_credentials.device_id();
      UMA_HISTOGRAM_ENUMERATION(policy::kMetricToken,
                                kMetricTokenLoadSucceeded,
                                policy::kMetricTokenSize);
    } else {
      UMA_HISTOGRAM_ENUMERATION(policy::kMetricToken,
                                kMetricTokenLoadFailed,
                                policy::kMetricTokenSize);
    }
  }

  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserPolicyTokenLoader::NotifyOnOriginThread,
                 this,
                 device_token,
                 device_id));
}

void UserPolicyTokenLoader::NotifyOnOriginThread(const std::string& token,
                                                 const std::string& device_id) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (delegate_.get())
    delegate_->OnTokenLoaded(token, device_id);
}

}  // namespace policy
