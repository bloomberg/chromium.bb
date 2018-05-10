// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/leveldb_site_characteristics_database.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace resource_coordinator {

namespace {

// Try to open a database (creates it if necessary).
// TODO(sebmarchand): Turn this into a callback that runs in TaskScheduler with
// MayBlock().
std::unique_ptr<leveldb::DB> OpenDatabase(const base::FilePath& db_path) {
  base::AssertBlockingAllowed();
  leveldb_env::Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  // TODO(sebmarchand): Do more validation here, try to repair the database if
  // it's corrupt and report some metrics.
  if (!status.ok()) {
    LOG(ERROR) << "Unable to open the Site Characteristics database: "
               << status.ToString();
    return nullptr;
  }
  return db;
}

}  // namespace

// static:
std::unique_ptr<LevelDBSiteCharacteristicsDatabase>
LevelDBSiteCharacteristicsDatabase::OpenOrCreateDatabase(
    const base::FilePath& db_path) {
  std::unique_ptr<leveldb::DB> db = OpenDatabase(db_path);
  if (!db)
    return nullptr;
  LevelDBSiteCharacteristicsDatabase* characteristics_db =
      new LevelDBSiteCharacteristicsDatabase(std::move(db), db_path);
  return base::WrapUnique(characteristics_db);
}

LevelDBSiteCharacteristicsDatabase::~LevelDBSiteCharacteristicsDatabase() =
    default;

void LevelDBSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDB(
    const std::string& site_origin,
    LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertBlockingAllowed();
  std::string protobuf_value;
  // TODO(sebmarchand): Move this to a separate thread as this is blocking.
  leveldb::Status s = db_->Get(read_options_, site_origin, &protobuf_value);
  base::Optional<SiteCharacteristicsProto> site_characteristic_proto;
  if (s.ok()) {
    site_characteristic_proto = SiteCharacteristicsProto();
    if (!site_characteristic_proto->ParseFromString(protobuf_value)) {
      site_characteristic_proto = base::nullopt;
      LOG(ERROR) << "Error while trying to parse a SiteCharacteristicsProto "
                 << "protobuf.";
    }
  }
  std::move(callback).Run(std::move(site_characteristic_proto));
}

void LevelDBSiteCharacteristicsDatabase::WriteSiteCharacteristicsIntoDB(
    const std::string& site_origin,
    const SiteCharacteristicsProto& site_characteristic_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertBlockingAllowed();
  // TODO(sebmarchand): Move this to a separate thread as this is blocking.
  leveldb::Status s = db_->Put(write_options_, site_origin,
                               site_characteristic_proto.SerializeAsString());
  if (!s.ok()) {
    LOG(ERROR) << "Error while inserting an element in the site characteristic "
               << "database: " << s.ToString();
  }
}

void LevelDBSiteCharacteristicsDatabase::RemoveSiteCharacteristicsFromDB(
    const std::vector<std::string>& site_origins) {
  base::AssertBlockingAllowed();
  // TODO(sebmarchand): Move this to a separate thread as this is blocking.
  leveldb::WriteBatch batch;
  for (const auto iter : site_origins)
    batch.Delete(iter);
  leveldb::Status status = db_->Write(write_options_, &batch);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to remove some entries from the site "
                 << "characteristics database: " << status.ToString();
  }
}

void LevelDBSiteCharacteristicsDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertBlockingAllowed();
  // TODO(sebmarchand): Move this to a separate thread as this is blocking.
  leveldb_env::Options options;
  db_.reset();
  leveldb::Status status = leveldb::DestroyDB(db_path_.AsUTF8Unsafe(), options);
  if (status.ok()) {
    db_ = OpenDatabase(db_path_);
  } else {
    LOG(WARNING) << "Failed to destroy the site characteristics database: "
                 << status.ToString();
  }
}

LevelDBSiteCharacteristicsDatabase::LevelDBSiteCharacteristicsDatabase(
    std::unique_ptr<leveldb::DB> db,
    const base::FilePath& db_path)
    : db_(std::move(db)), db_path_(db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Setting |sync| to false might cause some data loss if the system crashes
  // but it'll make the write operations faster (no data will be loss if only
  // the process crashes).
  write_options_.sync = false;
}

}  // namespace resource_coordinator
