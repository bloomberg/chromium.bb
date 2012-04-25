// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"

class FilePath;

namespace gdata {

class GDataEntry;
class GDataDBIter;

// GData Database interface class.
class GDataDB {
 public:
  enum Status {
    DB_OK = 0,
    DB_KEY_NOT_FOUND,  // Key not found.
    DB_CORRUPTION,  // Database file corrupt.
    DB_IO_ERROR,  // File I/O error.
    DB_INTERNAL_ERROR,
  };

  virtual ~GDataDB() {}

  // Puts |entry| to the database.
  virtual Status Put(const GDataEntry& entry) = 0;

  // Deletes a database entry with key |resource_id| or |path| respectively.
  virtual Status DeleteByResourceId(const std::string& resource_id) = 0;
  virtual Status DeleteByPath(const FilePath& path) = 0;

  // Fetches a GDataEntry* by key |resource_id| or |path| respectively.
  virtual Status GetByResourceId(const std::string& resource_id,
                                 scoped_ptr<GDataEntry>* entry) = 0;
  virtual Status GetByPath(const FilePath& path,
                           scoped_ptr<GDataEntry>* entry) = 0;

  // Creates an iterator to fetch all GDataEntry's under |path|.
  // Will not return NULL.
  virtual scoped_ptr<GDataDBIter> CreateIterator(const FilePath& path) = 0;

 protected:
  GDataDB() {}
};

// GData Database Iterator interface class.
class GDataDBIter {
 public:
  virtual ~GDataDBIter() {}

  // Fetches the next |entry| in the iteration sequence. Returns false when
  // there are no more entries.
  virtual bool GetNext(std::string* path, scoped_ptr<GDataEntry>* entry) = 0;

 protected:
  GDataDBIter() {}
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_H_
