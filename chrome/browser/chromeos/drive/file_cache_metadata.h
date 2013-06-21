// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace leveldb {
class DB;
class Iterator;
}  // namespace leveldb

namespace drive {
namespace internal {

// FileCacheMetadata maintains metadata of FileCache's cached files.
// This class only manages metadata. File operations are done by FileCache.
// All member access including ctor and dtor must be made on the blocking pool.
//
// OBSOLETE: This class is maintained only for importing old data.
// TODO(hashimoto): Remove this class at some point.
class FileCacheMetadata {
 public:
  // Result of Initialize().
  enum InitializeResult {
    INITIALIZE_FAILED,  // Could not open nor create DB.
    INITIALIZE_OPENED,  // Opened an existing DB.
    INITIALIZE_CREATED,  // Created a new DB.
  };

  // Object to iterate over entries stored in the cache metadata.
  class Iterator {
   public:
    explicit Iterator(scoped_ptr<leveldb::Iterator> it);
    ~Iterator();

    // Returns true if this iterator cannot advance any more and does not point
    // to a valid entry. GetKey(), GetValue() and Advance() should not be called
    // in such cases.
    bool IsAtEnd() const;

    // Returns the key of the entry currently pointed by this object.
    std::string GetKey() const;

    // Returns the value of the entry currently pointed by this object.
    const FileCacheEntry& GetValue() const;

    // Advances to the next entry.
    void Advance();

    // Returns true if this object has encountered any error.
    bool HasError() const;

   private:
    // Used to implement Advance().
    void AdvanceInternal();

    scoped_ptr<leveldb::Iterator> it_;
    FileCacheEntry entry_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  // Tests are allowed to pass NULL as |blocking_task_runner|.
  explicit FileCacheMetadata(base::SequencedTaskRunner* blocking_task_runner);

  ~FileCacheMetadata();

  // Initialize the cache metadata store. Returns true on success.
  InitializeResult Initialize(const base::FilePath& db_path);
  // Adds a new cache entry corresponding to |resource_id| if it doesn't
  // exist, otherwise update the existing entry.
  void AddOrUpdateCacheEntry(const std::string& resource_id,
                             const FileCacheEntry& cache_entry);

  // Removes entry corresponding to |resource_id| from cache map.
  void RemoveCacheEntry(const std::string& resource_id);

  // Gets the cache entry for file corresponding to |resource_id| and returns
  // true if entry exists in cache map.  Otherwise, returns false.
  bool GetCacheEntry(const std::string& resource_id, FileCacheEntry* entry);

  // Returns an object to iterate over entries.
  scoped_ptr<Iterator> GetIterator();

 private:
  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<leveldb::DB> level_db_;

  DISALLOW_COPY_AND_ASSIGN(FileCacheMetadata);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_
