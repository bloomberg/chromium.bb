// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace leveldb {
class DB;
class Iterator;
}

namespace drive {

class ResourceEntry;
class ResourceMetadataHeader;

// Storage for ResourceMetadata which is responsible to manage resource
// entries and child-parent relationships between entries.
class ResourceMetadataStorage {
 public:
  // This should be incremented when incompatibility change is made to DB
  // format.
  static const int kDBVersion = 6;

  class Iterator {
   public:
    explicit Iterator(scoped_ptr<leveldb::Iterator> it);
    ~Iterator();

    // Returns true if this iterator cannot advance any more.
    bool IsAtEnd() const;

    // Returns the entry currently pointed by this object.
    const ResourceEntry& Get() const;

    // Advances to the next entry.
    void Advance();

    // Returns true if this object has encountered any error.
    bool HasError() const;

   private:
    ResourceEntry entry_;
    scoped_ptr<leveldb::Iterator> it_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  explicit ResourceMetadataStorage(const base::FilePath& directory_path);
  virtual ~ResourceMetadataStorage();

  // Initializes this object.
  bool Initialize();

  // Sets the largest changestamp.
  bool SetLargestChangestamp(int64 largest_changestamp);

  // Gets the largest changestamp.
  int64 GetLargestChangestamp();

  // Puts the entry to this storage.
  bool PutEntry(const ResourceEntry& entry);

  // Returns an entry stored in this storage.
  scoped_ptr<ResourceEntry> GetEntry(const std::string& resource_id);

  // Removes an entry from this storage.
  bool RemoveEntry(const std::string& resource_id);

  // Returns an object to iterate over entries stored in this storage.
  scoped_ptr<Iterator> GetIterator();

  // Returns resource ID of the parent's child.
  std::string GetChild(const std::string& parent_resource_id,
                       const std::string& child_name);

  // Returns resource IDs of the parent's children.
  void GetChildren(const std::string& parent_resource_id,
                   std::vector<std::string>* children);

 private:
  friend class ResourceMetadataStorageTest;

  // Returns a string to be used as a key for child entry.
  static std::string GetChildEntryKey(const std::string& parent_resource_id,
                                      const std::string& child_name);

  // Puts header.
  bool PutHeader(const ResourceMetadataHeader& header);

  // Gets header.
  scoped_ptr<ResourceMetadataHeader> GetHeader();

  // Checks validity of the data.
  bool CheckValidity();

  // Path to the directory where the data is stored.
  base::FilePath directory_path_;

  // Entries stored in this storage.
  scoped_ptr<leveldb::DB> resource_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMetadataStorage);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_
