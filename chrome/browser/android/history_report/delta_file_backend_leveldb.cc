// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_backend_leveldb.h"

#include <inttypes.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

namespace {
const base::FilePath::CharType kDbFileName[] =
    FILE_PATH_LITERAL("DeltaFileLevelDb");

int64_t GetLastSeqNo(leveldb::DB* db) {
  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_iter(db->NewIterator(options));
  db_iter->SeekToLast();
  int64_t seq_no = 0;
  if (db_iter->Valid()) {
    history_report::DeltaFileEntry last_entry;
    leveldb::Slice value_slice = db_iter->value();
    if (last_entry.ParseFromArray(value_slice.data(), value_slice.size()))
      seq_no = last_entry.seq_no();
  }
  return seq_no;
}

void SaveChange(leveldb::DB* db,
                const std::string& url,
                const std::string& type) {
  int64_t seq_no = GetLastSeqNo(db) + 1;
  history_report::DeltaFileEntry entry;
  entry.set_seq_no(seq_no);
  entry.set_type(type);
  entry.set_url(url);
  leveldb::WriteOptions writeOptions;
  std::string key;
  base::SStringPrintf(&key, "%" PRId64, seq_no);
  leveldb::Status status = db->Put(
      writeOptions,
      leveldb::Slice(key),
      leveldb::Slice(entry.SerializeAsString()));
  if (!status.ok())
    LOG(WARNING) << "Save Change failed " << status.ToString();
}

}  // namespace

namespace history_report {

// Comparator used in leveldb.
class DeltaFileBackend::DigitsComparator : public leveldb::Comparator {
 public:
  int Compare(const leveldb::Slice& a,
              const leveldb::Slice& b) const override {
    int64_t first;
    int64_t second;
    // Keys which can't be parsed go to the end.
    if (!base::StringToInt64(a.ToString(), &first)) return 1;
    if (!base::StringToInt64(b.ToString(), &second)) return -1;
    if (first < second) return -1;
    if (first > second) return 1;
    return 0;
  }
  const char* Name() const override { return "DigitsComparator"; }
  void FindShortestSeparator(std::string*,
                                     const leveldb::Slice&) const override { }
  void FindShortSuccessor(std::string*) const override { }
};

DeltaFileBackend::DeltaFileBackend(const base::FilePath& dir)
    : path_(dir.Append(kDbFileName)),
      leveldb_cmp_(new DeltaFileBackend::DigitsComparator()) {
}

DeltaFileBackend::~DeltaFileBackend() {}

bool DeltaFileBackend::Init() {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum number of files.
  options.comparator = leveldb_cmp_.get();
  std::string path = path_.value();
  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.IsCorruption()) {
    LOG(WARNING) << "Deleting possibly-corrupt database";
    base::DeleteFile(path_, true);
    status = leveldb::DB::Open(options, path, &db);
  }
  if (status.ok()) {
    CHECK(db);
    db_.reset(db);
    return true;
  }
  LOG(WARNING) << "Unable to open " << path_.value() << ": "
               << status.ToString();
  return false;
}

bool DeltaFileBackend::EnsureInitialized() {
  if (db_.get()) return true;
  return Init();
}

void DeltaFileBackend::PageAdded(const GURL& url) {
  if (!EnsureInitialized()) return;
  SaveChange(db_.get(), url.spec().c_str(), "add");
}

void DeltaFileBackend::PageDeleted(const GURL& url) {
  if (!EnsureInitialized()) return;
  SaveChange(db_.get(), url.spec().c_str(), "del");
}

int64_t DeltaFileBackend::Trim(int64_t lower_bound) {
  if (!EnsureInitialized()) return -1;
  leveldb::ReadOptions read_options;
  std::unique_ptr<leveldb::Iterator> db_iter(db_->NewIterator(read_options));
  db_iter->SeekToFirst();
  if (!db_iter->Valid())
    return -1;
  history_report::DeltaFileEntry first_entry;
  leveldb::Slice value_slice = db_iter->value();
  if (!first_entry.ParseFromArray(value_slice.data(), value_slice.size()))
    return -1;
  int64_t min_seq_no = first_entry.seq_no();
  db_iter->SeekToLast();
  if (!db_iter->Valid())
    return -1;
  history_report::DeltaFileEntry last_entry;
  value_slice = db_iter->value();
  if (!last_entry.ParseFromArray(value_slice.data(), value_slice.size()))
    return -1;
  int64_t max_seq_no = last_entry.seq_no();
  // We want to have at least one entry in delta file left to know
  // last sequence number in SaveChange.
  if (max_seq_no <= lower_bound)
    lower_bound = max_seq_no - 1;
  leveldb::WriteBatch updates;
  for (int64_t seq_no = min_seq_no; seq_no <= lower_bound; ++seq_no) {
    std::string key;
    base::SStringPrintf(&key, "%" PRId64, seq_no);
    updates.Delete(leveldb::Slice(key));
  }

  leveldb::WriteOptions write_options;
  leveldb::Status status = db_->Write(write_options, &updates);
  if (status.ok())
    return max_seq_no;
  LOG(WARNING) << "Trim failed: " << status.ToString();
  return -1;
}

bool DeltaFileBackend::Recreate(const std::vector<std::string>& urls) {
  if (!EnsureInitialized()) return false;
  Clear();
  int64_t seq_no = 1;
  leveldb::WriteBatch updates;
  for (std::vector<std::string>::const_iterator it = urls.begin();
       it != urls.end();
       ++it) {
    DeltaFileEntry entry;
    entry.set_seq_no(seq_no);
    entry.set_url(*it);
    entry.set_type("add");
    std::string key;
    base::SStringPrintf(&key, "%" PRId64, seq_no);
    updates.Put(leveldb::Slice(key),
                leveldb::Slice(entry.SerializeAsString()));
    ++seq_no;
  }
  leveldb::WriteOptions options;
  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    return true;
  LOG(WARNING) << "Recreate failed: " << status.ToString();
  return false;
}

std::unique_ptr<std::vector<DeltaFileEntryWithData>> DeltaFileBackend::Query(
    int64_t last_seq_no,
    int32_t limit) {
  if (!EnsureInitialized())
    return base::MakeUnique<std::vector<DeltaFileEntryWithData>>();
  std::string start;
  base::SStringPrintf(&start, "%" PRId64, last_seq_no + 1);
  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_it(db_->NewIterator(options));
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result(
      new std::vector<DeltaFileEntryWithData>());
  int32_t count = 0;
  for (db_it->Seek(start); db_it->Valid() && count < limit; db_it->Next()) {
    DeltaFileEntry entry;
    leveldb::Slice value_slice = db_it->value();
    if (!entry.ParseFromArray(value_slice.data(), value_slice.size()))
      continue;
    result->push_back(DeltaFileEntryWithData(entry));
    ++count;
  }
  return result;
}

void DeltaFileBackend::Clear() {
  if (!EnsureInitialized()) return;
  db_.reset();
  base::DeleteFile(path_, true);
  Init();
}

std::string DeltaFileBackend::Dump() {
  std::string dump("\n Delta File [");
  if (!EnsureInitialized()) {
    dump.append("not initialized]");
    return dump;
  }
  dump.append("num pending entries=");
  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_it(db_->NewIterator(options));
  int num_entries = 0;
  for (db_it->SeekToFirst(); db_it->Valid(); db_it->Next()) num_entries++;
  dump.append(base::IntToString(num_entries));
  dump.append("]");
  return dump;
}

}  // namespace history_report
