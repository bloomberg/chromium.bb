// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/test_util.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace drive {

namespace test_util {

namespace {

// This class is used to monitor if any task is posted to a message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : posted_(false) {}
  virtual ~TaskObserver() {}

  // MessageLoop::TaskObserver overrides.
  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
  }
  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    posted_ = true;
  }

  // Returns true if any task was posted.
  bool posted() const { return posted_; }

 private:
  bool posted_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

}  // namespace

void RunBlockingPoolTask() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    base::MessageLoop::current()->AddTaskObserver(&task_observer);
    base::RunLoop().RunUntilIdle();
    base::MessageLoop::current()->RemoveTaskObserver(&task_observer);
    if (!task_observer.posted())
      break;
  }
}

void RegisterDrivePrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterBooleanPref(
      prefs::kDisableDrive,
      false);
  pref_registry->RegisterBooleanPref(
      prefs::kDisableDriveOverCellular,
      true);
  pref_registry->RegisterBooleanPref(
      prefs::kDisableDriveHostedFiles,
      false);
}

FakeNetworkChangeNotifier::FakeNetworkChangeNotifier()
    : type_(CONNECTION_WIFI) {
}

void FakeNetworkChangeNotifier::SetConnectionType(ConnectionType type) {
  type_ = type;
  NotifyObserversOfConnectionTypeChange();
}

net::NetworkChangeNotifier::ConnectionType
FakeNetworkChangeNotifier::GetCurrentConnectionType() const {
  return type_;
}

}  // namespace test_util
}  // namespace drive
