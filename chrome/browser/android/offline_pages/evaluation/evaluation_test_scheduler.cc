// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/evaluation/evaluation_test_scheduler.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "net/base/network_change_notifier.h"

namespace offline_pages {

namespace android {

namespace {

const char kLogTag[] = "EvaluationTestScheduler";

void StartProcessing();

void ProcessingDoneCallback(bool result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&StartProcessing));
}

void GetAllRequestsDone(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  if (requests.size() > 0) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    RequestCoordinator* coordinator =
        RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
    // TODO(romax) Maybe get current real condition.
    DeviceConditions device_conditions(
        true, 0, net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    coordinator->StartImmediateProcessing(device_conditions,
                                          base::Bind(&ProcessingDoneCallback));
  }
}

void StartProcessing() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
  coordinator->GetAllRequests(base::Bind(&GetAllRequestsDone));
}

}  // namespace

void EvaluationTestScheduler::Schedule(
    const TriggerConditions& trigger_conditions) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!coordinator_) {
    coordinator_ =
        RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
    // It's not expected that the coordinator would be nullptr since this bridge
    // would only be used for testing scenario.
    DCHECK(coordinator_);
  }
  coordinator_->GetLogger()->RecordActivity(std::string(kLogTag) +
                                            " Start schedule!");
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&StartProcessing));
}

void EvaluationTestScheduler::BackupSchedule(
    const TriggerConditions& trigger_conditions,
    long delay_in_seconds) {}

void EvaluationTestScheduler::Unschedule() {}

void EvaluationTestScheduler::ImmediateScheduleCallback(bool result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&StartProcessing));
}

}  // namespace android
}  // namespace offline_pages
