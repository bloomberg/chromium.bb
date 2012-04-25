// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_LEVELDB_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_LEVELDB_H_
#pragma once

#include <leveldb/db.h>
#include <string>

#include "base/file_path.h"
#include "chrome/browser/chromeos/gdata/gdata_db.h"

namespace gdata {

class GDataLevelDB : public GDataDB {
 public:
  GDataLevelDB();

  void Init(const FilePath& db_path);

 private:
  virtual ~GDataLevelDB();

  // GDataDB implementation.
  virtual Status Put(const GDataEntry& file) OVERRIDE;
  virtual Status DeleteByResourceId(const std::string& resource_id) OVERRIDE;
  virtual Status DeleteByPath(const FilePath& path) OVERRIDE;
  virtual Status GetByResourceId(const std::string& resource_id,
                                 scoped_ptr<GDataEntry>* file) OVERRIDE;
  virtual Status GetByPath(const FilePath& path,
                           scoped_ptr<GDataEntry>* file) OVERRIDE;
  virtual scoped_ptr<GDataDBIter> CreateIterator(const FilePath& path) OVERRIDE;

  // Returns |resource_id| for |path| by looking up path_db_.
  Status ResourceIdForPath(const FilePath& path, std::string* resource_id);

  scoped_ptr<leveldb::DB> level_db_;
};

class GDataLevelDBIter : public GDataDBIter {
 public:
  GDataLevelDBIter(scoped_ptr<leveldb::Iterator> level_db_iter,
                   GDataDB* db, const FilePath& path);
 private:
  virtual ~GDataLevelDBIter();

  // GDataDBIter implementation.
  virtual bool GetNext(std::string* path,
                       scoped_ptr<GDataEntry>* entry) OVERRIDE;

  scoped_ptr<leveldb::Iterator> level_db_iter_;
  GDataDB* db_;
  const FilePath path_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_LEVELDB_H_
