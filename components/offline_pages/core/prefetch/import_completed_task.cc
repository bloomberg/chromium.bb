// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/import_completed_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

bool UpdateToFinishedStateSync(int64_t offline_id,
                               bool success,
                               sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, error_code = ?"
      " WHERE offline_id = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(success ? PrefetchItemErrorCode::SUCCESS
                                  : PrefetchItemErrorCode::IMPORT_ERROR));
  statement.BindInt64(2, offline_id);
  statement.BindInt(3, static_cast<int>(PrefetchItemState::IMPORTING));

  return statement.Run();
}

}  // namespace

ImportCompletedTask::ImportCompletedTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    int64_t offline_id,
    bool success)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      offline_id_(offline_id),
      success_(success),
      weak_ptr_factory_(this) {}

ImportCompletedTask::~ImportCompletedTask() {}

void ImportCompletedTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&UpdateToFinishedStateSync, offline_id_, success_),
      base::BindOnce(&ImportCompletedTask::OnStateUpdatedToFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImportCompletedTask::OnStateUpdatedToFinished(bool success) {
  // No further action can be done if the database fails to be updated. The
  // cleanup task should eventually kick in to clean this up.
  if (success)
    prefetch_dispatcher_->SchedulePipelineProcessing();

  TaskComplete();
}

}  // namespace offline_pages
