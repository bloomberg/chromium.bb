// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/usage_reports_buffer_backend.h"

#include <inttypes.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/history_report/usage_report_util.h"
#include "chrome/browser/android/proto/delta_file.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {
const base::FilePath::CharType kBufferFileName[] =
    FILE_PATH_LITERAL("UsageReportsBuffer");
}  // namespace

namespace history_report {

UsageReportsBufferBackend::UsageReportsBufferBackend(const base::FilePath& dir)
    : db_file_name_(dir.Append(kBufferFileName)) {}

UsageReportsBufferBackend::~UsageReportsBufferBackend() {}

bool UsageReportsBufferBackend::Init() {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum number of files.
  std::string path = db_file_name_.value();
  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  UMA_HISTOGRAM_ENUMERATION("LevelDB.Open.UsageReportsBufferBackend",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  if (status.IsCorruption()) {
    LOG(ERROR) << "Deleting corrupt database";
    base::DeleteFile(db_file_name_, true);
    status = leveldb::DB::Open(options, path, &db);
  }
  if (status.ok()) {
    CHECK(db);
    db_.reset(db);
    return true;
  }
  LOG(WARNING) << "Unable to open " << path << ": "
               << status.ToString();
  return false;
}

void UsageReportsBufferBackend::AddVisit(const std::string& id,
    int64 timestamp_ms,
    bool typed_visit) {
  if (!db_.get()) {
    LOG(WARNING) << "AddVisit db not initilized.";
    return;
  }
  history_report::UsageReport report;
  report.set_id(id);
  report.set_timestamp_ms(timestamp_ms);
  report.set_typed_visit(typed_visit);
  leveldb::WriteOptions writeOptions;
  leveldb::Status status = db_->Put(
      writeOptions,
      leveldb::Slice(usage_report_util::ReportToKey(report)),
      leveldb::Slice(report.SerializeAsString()));
  if (!status.ok())
    LOG(WARNING) << "AddVisit failed " << status.ToString();
}

scoped_ptr<std::vector<UsageReport> >
UsageReportsBufferBackend::GetUsageReportsBatch(int batch_size) {
  scoped_ptr<std::vector<UsageReport> > reports(new std::vector<UsageReport>());
  if (!db_.get()) {
    return reports.Pass();
  }
  reports->reserve(batch_size);
  leveldb::ReadOptions options;
  scoped_ptr<leveldb::Iterator> db_iter(db_->NewIterator(options));
  db_iter->SeekToFirst();
  while (batch_size > 0 && db_iter->Valid()) {
    history_report::UsageReport last_report;
    leveldb::Slice value_slice = db_iter->value();
    if (last_report.ParseFromArray(value_slice.data(), value_slice.size())) {
      reports->push_back(last_report);
      --batch_size;
    }
    db_iter->Next();
  }
  return reports.Pass();
}

void UsageReportsBufferBackend::Remove(
    const std::vector<std::string>& reports) {
  if (!db_.get()) {
    return;
  }
  // TODO(haaawk): investigate if it's worth sorting the keys here to improve
  // performance.
  leveldb::WriteBatch updates;
  for (std::vector<std::string>::const_iterator it = reports.begin();
       it != reports.end();
       ++it) {
    updates.Delete(leveldb::Slice(*it));
  }

  leveldb::WriteOptions write_options;
  leveldb::Status status = db_->Write(write_options, &updates);
  if (!status.ok()) {
    LOG(WARNING) << "Remove failed: " << status.ToString();
  }
}

void UsageReportsBufferBackend::Clear() {
  db_.reset();
  base::DeleteFile(db_file_name_, true);
  Init();
}

std::string UsageReportsBufferBackend::Dump() {
  std::string dump("\n UsageReportsBuffer [");
  if (!db_.get()) {
    dump.append("not initialized]");
    return dump;
  }
  dump.append("num pending entries=");
  leveldb::ReadOptions options;
  int num_entries = 0;
  scoped_ptr<leveldb::Iterator> db_it(db_->NewIterator(options));
  for (db_it->SeekToFirst(); db_it->Valid(); db_it->Next()) num_entries++;
  dump.append(base::IntToString(num_entries));
  dump.append("]");
  return dump;
}

}  // namespace history_report

