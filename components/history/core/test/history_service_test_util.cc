// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_service_test_util.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/test_history_database.h"

namespace {

class QuitTask : public history::HistoryDBTask {
 public:
  QuitTask(const base::Closure& task) : task_(task) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override { task_.Run(); }

 private:
  ~QuitTask() override {}

  base::Closure task_;

  DISALLOW_COPY_AND_ASSIGN(QuitTask);
};

}  // namespace

namespace history {

scoped_ptr<HistoryService> CreateHistoryService(
    const base::FilePath& history_dir,
    const std::string& accept_languages,
    bool create_db) {
  scoped_ptr<HistoryService> history_service(new HistoryService());
  if (!history_service->Init(
          !create_db, accept_languages,
          history::TestHistoryDatabaseParamsForPath(history_dir))) {
    return nullptr;
  }

  if (create_db)
    BlockUntilHistoryProcessesPendingRequests(history_service.get());
  return history_service;
}

void BlockUntilHistoryProcessesPendingRequests(
    HistoryService* history_service) {
  base::RunLoop run_loop;
  base::CancelableTaskTracker tracker;
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(new QuitTask(run_loop.QuitClosure())),
      &tracker);
  run_loop.Run();

  // Spin the runloop again until idle.  The QuitTask above is destroyed via a
  // task posted to the current message loop, so spinning allows the task to be
  // properly destroyed.
  base::RunLoop().RunUntilIdle();
}

}  // namespace history
