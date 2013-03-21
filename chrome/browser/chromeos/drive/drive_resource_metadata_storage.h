// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_STORAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace drive {

class DriveEntryProto;
class DriveResourceMetadataHeader;

// Interface of a storage for DriveResourceMetadata which is responsible to
// manage entry info and child-parent relationships between entries.
// TODO(hashimoto): Add tests for this class.
// TODO(hashimoto): Implement DB version of this class.
// TODO(hashimoto): Move data save/load code to this class.
class DriveResourceMetadataStorage {
 public:
  virtual ~DriveResourceMetadataStorage() {}

  // Initializes this object.
  virtual bool Initialize() = 0;

  // Returns true if the data remains persistent even after this object's death.
  virtual bool IsPersistentStorage() = 0;

  // Sets the largest changestamp.
  virtual void SetLargestChangestamp(int64 largest_changestamp) = 0;

  // Gets the largest changestamp.
  virtual int64 GetLargestChangestamp() = 0;

  // Puts the entry to this storage.
  virtual void PutEntry(const DriveEntryProto& entry) = 0;

  // Returns an entry stored in this storage.
  virtual scoped_ptr<DriveEntryProto> GetEntry(
      const std::string& resource_id) = 0;

  // Removes an entry from this storage.
  virtual void RemoveEntry(const std::string& resource_id) = 0;

  // Puts child under the parent.
  virtual void PutChild(const std::string& parent_resource_id,
                        const base::FilePath::StringType& child_name,
                        const std::string& child_resource_id) = 0;

  // Returns resource ID of the parent's child.
  virtual std::string GetChild(
      const std::string& parent_resource_id,
      const base::FilePath::StringType& child_name) = 0;

  // Returns resource IDs of the parent's children.
  virtual void GetChildren(const std::string& parent_resource_id,
                           std::vector<std::string>* children) = 0;

  // Removes child from the parent.
  virtual void RemoveChild(const std::string& parent_resource_id,
                           const base::FilePath::StringType& child_name) = 0;
};

// Implementation of DriveResourceMetadataStorage on memory.
class DriveResourceMetadataStorageMemory
    : public DriveResourceMetadataStorage {
 public:
  DriveResourceMetadataStorageMemory();
  virtual ~DriveResourceMetadataStorageMemory();

  // DriveResourceMetadataStorage overrides:
  virtual bool Initialize() OVERRIDE;
  virtual bool IsPersistentStorage() OVERRIDE;
  virtual void SetLargestChangestamp(int64 largest_changestamp) OVERRIDE;
  virtual int64 GetLargestChangestamp() OVERRIDE;
  virtual void PutEntry(const DriveEntryProto& entry) OVERRIDE;
  virtual scoped_ptr<DriveEntryProto> GetEntry(
      const std::string& resource_id) OVERRIDE;
  virtual void RemoveEntry(const std::string& resource_id) OVERRIDE;
  virtual void PutChild(const std::string& parent_resource_id,
                        const base::FilePath::StringType& child_name,
                        const std::string& child_resource_id) OVERRIDE;
  virtual std::string GetChild(
      const std::string& parent_resource_id,
      const base::FilePath::StringType& child_name) OVERRIDE;
  virtual void GetChildren(const std::string& parent_resource_id,
                           std::vector<std::string>* children) OVERRIDE;
  virtual void RemoveChild(
      const std::string& parent_resource_id,
      const base::FilePath::StringType& child_name) OVERRIDE;

 private:
  // Map of resource id strings to DriveEntryProto.
  typedef std::map<std::string, DriveEntryProto> ResourceMap;

  // Map from base_name to resource id of child.
  typedef std::map<base::FilePath::StringType, std::string> ChildMap;

  // Map from resource id to ChildMap.
  typedef std::map<std::string, ChildMap> ChildMaps;

  int64 largest_changestamp_;

  // Entries stored in this storage.
  ResourceMap resource_map_;

  // Parent-child relationship between entries.
  ChildMaps child_maps_;

  DISALLOW_COPY_AND_ASSIGN(DriveResourceMetadataStorageMemory);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_STORAGE_H_
