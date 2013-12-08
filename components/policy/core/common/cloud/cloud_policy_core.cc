// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_core.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {

CloudPolicyCore::Observer::~Observer() {}

CloudPolicyCore::CloudPolicyCore(
    const PolicyNamespaceKey& key,
    CloudPolicyStore* store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : policy_ns_key_(key),
      store_(store),
      task_runner_(task_runner) {}

CloudPolicyCore::~CloudPolicyCore() {}

void CloudPolicyCore::Connect(scoped_ptr<CloudPolicyClient> client) {
  CHECK(!client_);
  CHECK(client);
  client_ = client.Pass();
  service_.reset(new CloudPolicyService(policy_ns_key_, client_.get(), store_));
  FOR_EACH_OBSERVER(Observer, observers_, OnCoreConnected(this));
}

void CloudPolicyCore::Disconnect() {
  if (client_)
    FOR_EACH_OBSERVER(Observer, observers_, OnCoreDisconnecting(this));
  refresh_delay_.reset();
  refresh_scheduler_.reset();
  service_.reset();
  client_.reset();
}

void CloudPolicyCore::RefreshSoon() {
  if (refresh_scheduler_)
    refresh_scheduler_->RefreshSoon();
}

void CloudPolicyCore::StartRefreshScheduler() {
  if (!refresh_scheduler_) {
    refresh_scheduler_.reset(
        new CloudPolicyRefreshScheduler(client_.get(), store_, task_runner_));
    UpdateRefreshDelayFromPref();
    FOR_EACH_OBSERVER(Observer, observers_, OnRefreshSchedulerStarted(this));
  }
}

void CloudPolicyCore::TrackRefreshDelayPref(
    PrefService* pref_service,
    const std::string& refresh_pref_name) {
  refresh_delay_.reset(new IntegerPrefMember());
  refresh_delay_->Init(
      refresh_pref_name.c_str(), pref_service,
      base::Bind(&CloudPolicyCore::UpdateRefreshDelayFromPref,
                 base::Unretained(this)));
  UpdateRefreshDelayFromPref();
}

void CloudPolicyCore::AddObserver(CloudPolicyCore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyCore::RemoveObserver(CloudPolicyCore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyCore::UpdateRefreshDelayFromPref() {
  if (refresh_scheduler_ && refresh_delay_)
    refresh_scheduler_->SetRefreshDelay(refresh_delay_->GetValue());
}

}  // namespace policy
