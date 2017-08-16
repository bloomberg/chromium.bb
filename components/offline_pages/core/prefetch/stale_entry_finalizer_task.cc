// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/stale_entry_finalizer_task.h"

#include <array>

#include "base/bind.h"
#include "components/offline_pages/core/offline_time_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

const base::TimeDelta FreshnessPeriodForState(PrefetchItemState state) {
  switch (state) {
    // Bucket 1.
    case PrefetchItemState::NEW_REQUEST:
      return base::TimeDelta::FromDays(1);
    // Bucket 2.
    case PrefetchItemState::AWAITING_GCM:
    case PrefetchItemState::RECEIVED_GCM:
    case PrefetchItemState::RECEIVED_BUNDLE:
      return base::TimeDelta::FromDays(1);
    // Bucket 3.
    case PrefetchItemState::DOWNLOADING:
      return base::TimeDelta::FromDays(2);
    default:
      NOTREACHED();
  }
  return base::TimeDelta::FromDays(1);
}

PrefetchItemErrorCode ErrorCodeForState(PrefetchItemState state) {
  switch (state) {
    case PrefetchItemState::NEW_REQUEST:
      return PrefetchItemErrorCode::STALE_AT_NEW_REQUEST;
    case PrefetchItemState::AWAITING_GCM:
      return PrefetchItemErrorCode::STALE_AT_AWAITING_GCM;
    case PrefetchItemState::RECEIVED_GCM:
      return PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM;
    case PrefetchItemState::RECEIVED_BUNDLE:
      return PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE;
    case PrefetchItemState::DOWNLOADING:
      return PrefetchItemErrorCode::STALE_AT_DOWNLOADING;
    default:
      NOTREACHED();
  }
  return PrefetchItemErrorCode::STALE_AT_UNKNOWN;
}

bool FinalizeStaleItems(PrefetchItemState state,
                        base::Time now,
                        sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE state = ? AND freshness_time < ?";
  const int64_t earliest_fresh_db_time =
      ToDatabaseTime(now - FreshnessPeriodForState(state));
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(ErrorCodeForState(state)));
  statement.BindInt(2, static_cast<int>(state));
  statement.BindInt64(3, earliest_fresh_db_time);

  return statement.Run();
}

bool FinalizeStaleEntriesSync(StaleEntryFinalizerTask::NowGetter now_getter,
                              sql::Connection* db) {
  if (!db)
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  const std::array<PrefetchItemState, 5> expirable_states = {{
      // Bucket 1.
      PrefetchItemState::NEW_REQUEST,
      // Bucket 2.
      PrefetchItemState::AWAITING_GCM, PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::RECEIVED_BUNDLE,
      // Bucket 3.
      PrefetchItemState::DOWNLOADING,
  }};
  base::Time now = now_getter.Run();
  for (PrefetchItemState state : expirable_states) {
    if (!FinalizeStaleItems(state, now, db))
      return false;
  }

  // If all FinalizeStaleItems calls succeeded the transaction is committed.
  return transaction.Commit();
}

}  // namespace

StaleEntryFinalizerTask::StaleEntryFinalizerTask(PrefetchStore* prefetch_store)
    : prefetch_store_(prefetch_store),
      now_getter_(base::BindRepeating(&base::Time::Now)),
      weak_ptr_factory_(this) {
  DCHECK(prefetch_store_);
}

StaleEntryFinalizerTask::~StaleEntryFinalizerTask() {}

void StaleEntryFinalizerTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&FinalizeStaleEntriesSync, now_getter_),
      base::BindOnce(&StaleEntryFinalizerTask::OnFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StaleEntryFinalizerTask::SetNowGetterForTesting(NowGetter now_getter) {
  now_getter_ = now_getter;
}

void StaleEntryFinalizerTask::OnFinished(bool success) {
  ran_successfully_ = success;
  TaskComplete();
}

}  // namespace offline_pages
