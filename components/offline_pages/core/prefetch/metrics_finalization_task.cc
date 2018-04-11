// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/metrics_finalization_task.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

// In-memory representation of URL metadata fetched from SQLite storage.
struct PrefetchItemStats {
  PrefetchItemStats(int64_t offline_id,
                    int generate_bundle_attempts,
                    int get_operation_attempts,
                    int download_initiation_attempts,
                    int64_t archive_body_length,
                    base::Time creation_time,
                    PrefetchItemErrorCode error_code,
                    int64_t file_size)
      : offline_id(offline_id),
        generate_bundle_attempts(generate_bundle_attempts),
        get_operation_attempts(get_operation_attempts),
        download_initiation_attempts(download_initiation_attempts),
        archive_body_length(archive_body_length),
        creation_time(creation_time),
        error_code(error_code),
        file_size(file_size) {}

  int64_t offline_id;
  int generate_bundle_attempts;
  int get_operation_attempts;
  int download_initiation_attempts;
  int64_t archive_body_length;
  base::Time creation_time;
  PrefetchItemErrorCode error_code;
  int64_t file_size;
};

std::vector<PrefetchItemStats> FetchUrlsSync(sql::Connection* db) {
  static const char kSql[] = R"(
  SELECT offline_id, generate_bundle_attempts, get_operation_attempts,
    download_initiation_attempts, archive_body_length, creation_time,
    error_code, file_size
  FROM prefetch_items
  WHERE state = ?
)";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));

  std::vector<PrefetchItemStats> urls;
  while (statement.Step()) {
    urls.emplace_back(statement.ColumnInt64(0),  // offline_id
                      statement.ColumnInt(1),    // generate_bundle_attempts
                      statement.ColumnInt(2),    // get_operation_attempts
                      statement.ColumnInt(3),    // download_initiation_attempts
                      statement.ColumnInt64(4),  // archive_body_length
                      store_utils::FromDatabaseTime(
                          statement.ColumnInt64(5)),  // creation_time
                      static_cast<PrefetchItemErrorCode>(
                          statement.ColumnInt(6)),  // error_code
                      statement.ColumnInt64(7)      // file_size
                      );
  }

  return urls;
}

bool MarkUrlAsZombie(sql::Connection* db,
                     base::Time freshness_time,
                     int64_t offline_id) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, freshness_time = ? WHERE "
      "offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::ZOMBIE));
  statement.BindInt(1, store_utils::ToDatabaseTime(freshness_time));
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

void LogStateCountMetrics(PrefetchItemState state, int count) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for setting a count.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      "OfflinePages.Prefetching.StateCounts",
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(static_cast<int>(state), count);
}

void CountEntriesInEachState(sql::Connection* db) {
  static const char kSql[] =
      "SELECT state, COUNT (*) FROM prefetch_items GROUP BY state";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  while (statement.Step()) {
    PrefetchItemState state =
        static_cast<PrefetchItemState>(statement.ColumnInt(0));
    int count = statement.ColumnInt(1);
    LogStateCountMetrics(state, count);
  }
}

// These constants represent important indexes in the
// OfflinePrefetchArchiveActualSizeVsExpected histogram enum.
const int kEnumZeroArchiveBodyLength = 0;
const int kEnumRange0To100Min = 1;
const int kEnumRange0To100Max = 10;
const int kEnumSizeMatches100 = 11;
const int kEnumRange100To200OrMoreMin = 12;
const int kEnumRange100To200OrMoreMax = 22;

// Value meaning the file size should not be reported (not a valid index in the
// OfflinePrefetchArchiveActualSizeVsExpected enum).
const int kShouldNotReportFileSize = -1;

// Returned values conform to the definition of the
// OfflinePrefetchArchiveActualSizeVsExpected histogram enum. Except for -1
// which means "do not report".
int GetFileSizeEnumValueFor(int64_t archive_body_length, int64_t file_size) {
  // Archiving service reported body length as zero.
  if (archive_body_length == 0)
    return kEnumZeroArchiveBodyLength;

  // For other cases, reporting should only happen if both size values were set
  // meaning the item reached at least the successfully downloaded state.
  if (archive_body_length < 0 || file_size < 0)
    return kShouldNotReportFileSize;

  if (archive_body_length == file_size)
    return kEnumSizeMatches100;

  // For smaller sizes than expected, return an index in the 0% <= ratio < 100%
  // range.
  double ratio = static_cast<double>(file_size) / archive_body_length;
  if (archive_body_length > file_size) {
    return std::max(
        kEnumRange0To100Min,
        std::min(kEnumRange0To100Max, static_cast<int>(ratio * 10) + 1));
  }

  // Otherwise return an index in the 100% < ratio range.
  return std::max(
      kEnumRange100To200OrMoreMin,
      std::min(kEnumRange100To200OrMoreMax, static_cast<int>(ratio * 10) + 2));
}

void ReportMetricsFor(const PrefetchItemStats& url, const base::Time now) {
  // Lifetime reporting.
  static const int kFourWeeksInSeconds =
      base::TimeDelta::FromDays(28).InSeconds();
  const bool successful = url.error_code == PrefetchItemErrorCode::SUCCESS;
  int64_t lifetime_seconds = (now - url.creation_time).InSeconds();
  if (successful) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.Prefetching.ItemLifetime.Successful", lifetime_seconds, 1,
        kFourWeeksInSeconds, 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.Prefetching.ItemLifetime.Failed",
                                lifetime_seconds, 1, kFourWeeksInSeconds, 50);
  }

  // Error code reporting.
  base::UmaHistogramSparse("OfflinePages.Prefetching.FinishedItemErrorCode",
                           static_cast<int>(url.error_code));

  // Unexpected file size reporting.
  int file_size_enum_value =
      GetFileSizeEnumValueFor(url.archive_body_length, url.file_size);
  if (file_size_enum_value != kShouldNotReportFileSize) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Prefetching.DownloadedArchiveSizeVsExpected",
        file_size_enum_value, kEnumRange100To200OrMoreMax);
  }

  // Attempt counts reporting.
  static const int kMaxPossibleRetries = 20;
  UMA_HISTOGRAM_EXACT_LINEAR(
      "OfflinePages.Prefetching.ActionAttempts.GeneratePageBundle",
      url.generate_bundle_attempts, kMaxPossibleRetries);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "OfflinePages.Prefetching.ActionAttempts.GetOperation",
      url.get_operation_attempts, kMaxPossibleRetries);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "OfflinePages.Prefetching.ActionAttempts.DownloadInitiation",
      url.download_initiation_attempts, kMaxPossibleRetries);
}

bool ReportMetricsAndFinalizeSync(sql::Connection* db) {
  if (!db)
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Gather metrics about the current number of entries with each state.  Check
  // before zombification so that we will be able to see entries in the finished
  // state, otherwise we will only see zombies, never finished entries.
  CountEntriesInEachState(db);

  const std::vector<PrefetchItemStats> urls = FetchUrlsSync(db);

  base::Time now = base::Time::Now();
  for (const auto& url : urls) {
    MarkUrlAsZombie(db, now, url.offline_id);
  }

  if (transaction.Commit()) {
    for (const auto& url : urls) {
      DVLOG(1) << "Finalized prefetch item with error code "
               << static_cast<int>(url.error_code) << ": (" << url.offline_id
               << ", " << url.generate_bundle_attempts << ", "
               << url.get_operation_attempts << ", "
               << url.download_initiation_attempts << ", "
               << url.archive_body_length << ", " << url.creation_time << ", "
               << url.file_size << ")";
      ReportMetricsFor(url, now);
    }
    return true;
  }

  return false;
}

}  // namespace

MetricsFinalizationTask::MetricsFinalizationTask(PrefetchStore* prefetch_store)
    : prefetch_store_(prefetch_store), weak_factory_(this) {}

MetricsFinalizationTask::~MetricsFinalizationTask() {}

void MetricsFinalizationTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&ReportMetricsAndFinalizeSync),
      base::BindOnce(&MetricsFinalizationTask::MetricsFinalized,
                     weak_factory_.GetWeakPtr()));
}

void MetricsFinalizationTask::MetricsFinalized(bool result) {
  TaskComplete();
}

}  // namespace offline_pages
