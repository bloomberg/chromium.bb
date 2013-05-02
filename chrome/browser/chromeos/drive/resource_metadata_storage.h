// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_STORAGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace leveldb {
class DB;
}

namespace drive {

class ResourceEntry;
class ResourceMetadataHeader;

typedef base::Callback<void(const ResourceEntry& entry)> IterateCallback;

// Storage for ResourceMetadata which is responsible to manage entry info
// and child-parent relationships between entries.
class ResourceMetadataStorage {
 public:
  // This should be incremented when incompatibility change is made to DB
  // format.
  static const int kDBVersion = 5;

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

  // Iterates over entries stored in this storage.
  void Iterate(const IterateCallback& callback);

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
