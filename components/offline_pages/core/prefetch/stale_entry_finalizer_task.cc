// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/stale_entry_finalizer_task.h"

#include <array>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

using Result = StaleEntryFinalizerTask::Result;

namespace {

// If this time changes, we need to update the desciption in histograms.xml
// for OfflinePages.Prefetching.StuckItemState.
const int kStuckTimeLimitInDays = 7;

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
    case PrefetchItemState::IMPORTING:
      return kPrefetchDownloadLifetime;
    // The following states do not expire based on per bucket freshness so they
    // are not expected to be passed into this function.
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
    case PrefetchItemState::SENT_GET_OPERATION:
    case PrefetchItemState::DOWNLOADED:
    case PrefetchItemState::FINISHED:
    case PrefetchItemState::ZOMBIE:
      NOTREACHED();
  }
  return base::TimeDelta::FromDays(1);
}

PrefetchItemErrorCode ErrorCodeForState(PrefetchItemState state) {
  switch (state) {
    // Valid values.
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
    case PrefetchItemState::IMPORTING:
      return PrefetchItemErrorCode::STALE_AT_IMPORTING;
    // The following states do not expire based on per bucket freshness so they
    // are not expected to be passed into this function.
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
    case PrefetchItemState::SENT_GET_OPERATION:
    case PrefetchItemState::DOWNLOADED:
    case PrefetchItemState::FINISHED:
    case PrefetchItemState::ZOMBIE:
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
      store_utils::ToDatabaseTime(now - FreshnessPeriodForState(state));
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(ErrorCodeForState(state)));
  statement.BindInt(2, static_cast<int>(state));
  statement.BindInt64(3, earliest_fresh_db_time);

  return statement.Run();
}

bool MoreWorkInQueue(sql::Connection* db) {
  static const char kSql[] =
      "SELECT COUNT(*) FROM prefetch_items"
      " WHERE state NOT IN (?, ?)";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::ZOMBIE));
  statement.BindInt(1, static_cast<int>(PrefetchItemState::AWAITING_GCM));

  // In event of failure, assume more work exists.
  if (!statement.Step())
    return true;

  return statement.ColumnInt(0) > 0;
}

// If the user shifted the clock backwards too far, our items will stay around
// for a very long time.  Don't allow that so we don't waste resources with
// potentially outdated content.
bool FinalizeFutureItems(PrefetchItemState state,
                         base::Time now,
                         sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE state = ? AND freshness_time > ?";
  const int64_t future_fresh_db_time_limit =
      store_utils::ToDatabaseTime(now + base::TimeDelta::FromDays(1));
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(
             PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED));
  statement.BindInt(2, static_cast<int>(state));
  statement.BindInt64(3, future_fresh_db_time_limit);

  return statement.Run();
}

// If there is a bug in our code, an item might be stuck in the queue waiting
// on an event that didn't happen.  If so, report that item.
void ReportStuckItems(base::Time now, sql::Connection* db) {
  static constexpr char kSql[] =
      "SELECT state FROM prefetch_items"
      " WHERE creation_time < ?";
  const int64_t earliest_valid_creation_time = store_utils::ToDatabaseTime(
      now - base::TimeDelta::FromDays(kStuckTimeLimitInDays));
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, earliest_valid_creation_time);

  while (statement.Step()) {
    base::UmaHistogramSparse("OfflinePages.Prefetching.StuckItemState",
                             statement.ColumnInt(0));
  }
}

Result FinalizeStaleEntriesSync(StaleEntryFinalizerTask::NowGetter now_getter,
                                sql::Connection* db) {
  if (!db)
    return Result::NO_MORE_WORK;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return Result::NO_MORE_WORK;

  // Only the following states are supposed to expire based on per bucket
  // freshness.
  static constexpr std::array<PrefetchItemState, 6> expirable_states = {{
      // Bucket 1.
      PrefetchItemState::NEW_REQUEST,
      // Bucket 2.
      PrefetchItemState::AWAITING_GCM, PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::RECEIVED_BUNDLE,
      // Bucket 3.
      PrefetchItemState::DOWNLOADING, PrefetchItemState::IMPORTING,
  }};
  base::Time now = now_getter.Run();
  for (PrefetchItemState state : expirable_states) {
    if (!FinalizeStaleItems(state, now, db))
      return Result::NO_MORE_WORK;

    if (!FinalizeFutureItems(state, now, db))
      return Result::NO_MORE_WORK;
  }

  // Items could also be stuck in a non-expirable state due to a bug, report
  // them.
  ReportStuckItems(now, db);

  Result result = Result::MORE_WORK_NEEDED;
  if (!MoreWorkInQueue(db))
    result = Result::NO_MORE_WORK;

  // If all FinalizeStaleItems calls succeeded the transaction is committed.
  return transaction.Commit() ? result : Result::NO_MORE_WORK;
}

}  // namespace

StaleEntryFinalizerTask::StaleEntryFinalizerTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      now_getter_(base::BindRepeating(&base::Time::Now)),
      weak_ptr_factory_(this) {
  DCHECK(prefetch_dispatcher_);
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

void StaleEntryFinalizerTask::OnFinished(Result result) {
  final_status_ = result;
  if (final_status_ == Result::MORE_WORK_NEEDED)
    prefetch_dispatcher_->EnsureTaskScheduled();
  DVLOG(1) << "Finalization task "
           << (result == Result::NO_MORE_WORK ? "not " : "")
           << "scheduling background processing.";
  TaskComplete();
}

}  // namespace offline_pages
