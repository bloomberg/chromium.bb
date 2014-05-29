// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/drive/drive_service_interface.h"

namespace base {
class SequencedTaskRunner;
}

namespace leveldb {
class DB;
class Iterator;
}

namespace drive {

class ResourceEntry;
class ResourceMetadataHeader;

namespace internal {

// Storage for ResourceMetadata which is responsible to manage resource
// entries and child-parent relationships between entries.
class ResourceMetadataStorage {
 public:
  // This should be incremented when incompatibility change is made to DB
  // format.
  static const int kDBVersion = 13;

  // Object to iterate over entries stored in this storage.
  class Iterator {
   public:
    explicit Iterator(scoped_ptr<leveldb::Iterator> it);
    ~Iterator();

    // Returns true if this iterator cannot advance any more and does not point
    // to a valid entry. Get() and Advance() should not be called in such cases.
    bool IsAtEnd() const;

    // Returns the ID of the entry currently pointed by this object.
    std::string GetID() const;

    // Returns the entry currently pointed by this object.
    const ResourceEntry& GetValue() const;

    // Advances to the next entry.
    void Advance();

    // Returns true if this object has encountered any error.
    bool HasError() const;

   private:
    ResourceEntry entry_;
    scoped_ptr<leveldb::Iterator> it_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  // Cache information recovered from trashed DB.
  struct RecoveredCacheInfo {
    RecoveredCacheInfo();
    ~RecoveredCacheInfo();

    bool is_dirty;
    std::string md5;
    std::string title;
  };
  typedef std::map<std::string, RecoveredCacheInfo> RecoveredCacheInfoMap;

  // Returns true if the DB was successfully upgraded to the newest version.
  static bool UpgradeOldDB(const base::FilePath& directory_path,
                           const ResourceIdCanonicalizer& id_canonicalizer);

  ResourceMetadataStorage(const base::FilePath& directory_path,
                          base::SequencedTaskRunner* blocking_task_runner);

  const base::FilePath& directory_path() const { return directory_path_; }

  // Returns true when cache entries were not loaded to the DB during
  // initialization.
  bool cache_file_scan_is_needed() const { return cache_file_scan_is_needed_; }

  // Destroys this object.
  void Destroy();

  // Initializes this object.
  bool Initialize();

  // Collects cache info from trashed resource map DB.
  void RecoverCacheInfoFromTrashedResourceMap(RecoveredCacheInfoMap* out_info);

  // Sets the largest changestamp.
  FileError SetLargestChangestamp(int64 largest_changestamp);

  // Gets the largest changestamp.
  FileError GetLargestChangestamp(int64* largest_changestamp);

  // Puts the entry to this storage.
  FileError PutEntry(const ResourceEntry& entry);

  // Gets an entry stored in this storage.
  FileError GetEntry(const std::string& id, ResourceEntry* out_entry);

  // Removes an entry from this storage.
  FileError RemoveEntry(const std::string& id);

  // Returns an object to iterate over entries stored in this storage.
  scoped_ptr<Iterator> GetIterator();

  // Returns the ID of the parent's child.
  FileError GetChild(const std::string& parent_id,
                     const std::string& child_name,
                     std::string* child_id);

  // Returns the IDs of the parent's children.
  FileError GetChildren(const std::string& parent_id,
                        std::vector<std::string>* children);

  // Returns the local ID associated with the given resource ID.
  FileError GetIdByResourceId(const std::string& resource_id,
                              std::string* out_id);

 private:
  friend class ResourceMetadataStorageTest;

  // To destruct this object, use Destroy().
  ~ResourceMetadataStorage();

  // Used to implement Destroy().
  void DestroyOnBlockingPool();

  // Returns a string to be used as a key for child entry.
  static std::string GetChildEntryKey(const std::string& parent_id,
                                      const std::string& child_name);

  // Puts header.
  FileError PutHeader(const ResourceMetadataHeader& header);

  // Gets header.
  FileError GetHeader(ResourceMetadataHeader* out_header);

  // Checks validity of the data.
  bool CheckValidity();

  // Path to the directory where the data is stored.
  base::FilePath directory_path_;

  bool cache_file_scan_is_needed_;

  // Entries stored in this storage.
  scoped_ptr<leveldb::DB> resource_map_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMetadataStorage);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_
