// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace resource_coordinator {

// Manages a LevelDB database used by a site characteristic data store.
// TODO(sebmarchand):
//   - Constraint the size of the database: Use a background task to trim the
//     database if it becomes too big and ensure that this fail nicely when the
//     disk is full.
//   - Batch the write operations to reduce the number of I/O events.
//   - Move the I/O operations to a different thread.
//
// All the DB operations need to be done on the I/O thread.
class LevelDBSiteCharacteristicsDatabase
    : public LocalSiteCharacteristicsDatabase {
 public:
  // Initialize the database, create it if it doesn't exist or just open it if
  // it does. The on-disk database will be stored in |db_path|.
  static std::unique_ptr<LevelDBSiteCharacteristicsDatabase>
  OpenOrCreateDatabase(const base::FilePath& db_path);

  ~LevelDBSiteCharacteristicsDatabase() override;

  // LocalSiteCharacteristicDatabase:
  void ReadSiteCharacteristicsFromDB(
      const std::string& site_origin,
      LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
          callback) override;
  void WriteSiteCharacteristicsIntoDB(
      const std::string& site_origin,
      const SiteCharacteristicsProto& site_characteristic_proto) override;
  void RemoveSiteCharacteristicsFromDB(
      const std::vector<std::string>& site_origin) override;
  void ClearDatabase() override;

 private:
  LevelDBSiteCharacteristicsDatabase(std::unique_ptr<leveldb::DB> db,
                                     const base::FilePath& db_path);

  // The connection to the LevelDB database.
  std::unique_ptr<leveldb::DB> db_;
  // The options to be used for all database read operations.
  leveldb::ReadOptions read_options_;
  // The options to be used for all database write operations.
  leveldb::WriteOptions write_options_;

  // The on disk location of the database.
  const base::FilePath db_path_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(LevelDBSiteCharacteristicsDatabase);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_
