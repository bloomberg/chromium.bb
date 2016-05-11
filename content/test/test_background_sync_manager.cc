// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_background_sync_manager.h"

#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TestBackgroundSyncManager::TestBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : BackgroundSyncManager(service_worker_context) {}

TestBackgroundSyncManager::~TestBackgroundSyncManager() {}

void TestBackgroundSyncManager::DoInit() {
  Init();
}

void TestBackgroundSyncManager::ResumeBackendOperation() {
  ASSERT_FALSE(continuation_.is_null());
  continuation_.Run();
  continuation_.Reset();
}

void TestBackgroundSyncManager::ClearDelayedTask() {
  delayed_task_.Reset();
}

void TestBackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
    const GURL& origin,
    const std::string& key,
    const std::string& data,
    const ServiceWorkerStorage::StatusCallback& callback) {
  EXPECT_TRUE(continuation_.is_null());
  if (corrupt_backend_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }
  continuation_ = base::Bind(
      &TestBackgroundSyncManager::StoreDataInBackendContinue,
      base::Unretained(this), sw_registration_id, origin, key, data, callback);
  if (delay_backend_)
    return;

  ResumeBackendOperation();
}

void TestBackgroundSyncManager::GetDataFromBackend(
    const std::string& key,
    const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
        callback) {
  EXPECT_TRUE(continuation_.is_null());
  if (corrupt_backend_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
                   SERVICE_WORKER_ERROR_FAILED));
    return;
  }
  continuation_ =
      base::Bind(&TestBackgroundSyncManager::GetDataFromBackendContinue,
                 base::Unretained(this), key, callback);
  if (delay_backend_)
    return;

  ResumeBackendOperation();
}

void TestBackgroundSyncManager::DispatchSyncEvent(
    const std::string& tag,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    blink::mojom::BackgroundSyncEventLastChance last_chance,
    const ServiceWorkerVersion::StatusCallback& callback) {
  ASSERT_FALSE(dispatch_sync_callback_.is_null());
  last_chance_ = last_chance;
  dispatch_sync_callback_.Run(active_version, callback);
}

void TestBackgroundSyncManager::ScheduleDelayedTask(
    const base::Closure& callback,
    base::TimeDelta delay) {
  delayed_task_ = callback;
  delayed_task_delta_ = delay;
}

void TestBackgroundSyncManager::HasMainFrameProviderHost(
    const GURL& origin,
    const BoolCallback& callback) {
  callback.Run(has_main_frame_provider_host_);
}

void TestBackgroundSyncManager::StoreDataInBackendContinue(
    int64_t sw_registration_id,
    const GURL& origin,
    const std::string& key,
    const std::string& data,
    const ServiceWorkerStorage::StatusCallback& callback) {
  BackgroundSyncManager::StoreDataInBackend(sw_registration_id, origin, key,
                                            data, callback);
}

void TestBackgroundSyncManager::GetDataFromBackendContinue(
    const std::string& key,
    const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
        callback) {
  BackgroundSyncManager::GetDataFromBackend(key, callback);
}

}  // namespace content
