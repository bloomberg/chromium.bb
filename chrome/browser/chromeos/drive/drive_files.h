// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

class DriveDirectory;
class DriveResourceMetadata;

// Used to read a directory from the file system.
// If |error| is not DRIVE_FILE_OK, |entries| is set to NULL.
// |entries| are contents, both files and directories, of the directory.
typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

// Base class for representing files and directories in Drive virtual file
// system.
class DriveEntry {
 public:
  virtual ~DriveEntry();

  virtual DriveDirectory* AsDriveDirectory();

  const DriveEntryProto& proto() const { return proto_; }

  // Copies values from proto.
  void FromProto(const DriveEntryProto& proto);

  // This is not the full path, use GetFilePath for that.
  // Note that base_name_ gets reset by SetBaseNameFromTitle() in a number of
  // situations due to de-duplication (see AddEntry).
  const base::FilePath::StringType& base_name() const {
    return proto_.base_name();
  }
  // TODO(achuith): Make this private when GDataDB no longer uses path as a key.
  void set_base_name(const base::FilePath::StringType& name) {
    proto_.set_base_name(name);
  }

  const base::FilePath::StringType& title() const { return proto_.title(); }
  void set_title(const base::FilePath::StringType& title) {
    proto_.set_title(title);
  }

  // The unique resource ID associated with this file system entry.
  const std::string& resource_id() const { return proto_.resource_id(); }
  void set_resource_id(const std::string& resource_id) {
    proto_.set_resource_id(resource_id);
  }

  // The resource id of the parent folder. This piece of information is needed
  // to pair files from change feeds with their directory parents within the
  // existing file system snapshot (DriveResourceMetadata::resource_map_).
  const std::string& parent_resource_id() const {
    return proto_.parent_resource_id();
  }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of DriveFileSystem
  // class.
  base::FilePath GetFilePath() const;

  // Sets |base_name_| based on the value of |title_| without name
  // de-duplication (see AddEntry() for details on de-duplication).
  virtual void SetBaseNameFromTitle();

 protected:
  friend class DriveResourceMetadata;  // For access to ctor.
  // For access to set_parent_resource_id() from AddEntry.
  friend class DriveDirectory;

  explicit DriveEntry(DriveResourceMetadata* resource_metadata);

  // Sets the parent directory of this file system entry.
  // It is intended to be used by DriveDirectory::AddEntry() only.
  void set_parent_resource_id(const std::string& parent_resource_id);

  DriveEntryProto proto_;

  // Weak pointer to DriveResourceMetadata.
  DriveResourceMetadata* resource_metadata_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveEntry);
};

// Represents "directory" in a drive virtual file system. Maps to drive
// collection element.
class DriveDirectory : public DriveEntry {
 public:
  virtual ~DriveDirectory();

  // Converts to/from proto.
  void FromProto(const DriveDirectoryProto& proto);
  void ToProto(DriveDirectoryProto* proto) const;

  // Converts the children as a vector of DriveEntryProto.
  scoped_ptr<DriveEntryProtoVector> ToProtoVector() const;

  // Returns the changestamp of this directory. See drive.proto for details.
  int64 changestamp() const;
  void set_changestamp(int64 changestamp);

 private:
  // TODO(satorux): Remove the friend statements. crbug.com/139649
  friend class DriveResourceMetadata;
  friend class DriveResourceMetadataTest;
  friend class ChangeListProcessor;

  explicit DriveDirectory(DriveResourceMetadata* resource_metadata);
  virtual DriveDirectory* AsDriveDirectory() OVERRIDE;

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name de-duplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  // TODO(satorux): Remove this. crbug.com/139649
  void AddEntry(DriveEntry* entry);

  // Removes the entry from its children list and destroys the entry instance.
  // TODO(satorux): Remove this. crbug.com/139649
  void RemoveEntry(DriveEntry* entry);

  // Takes over all entries from |dir|.
  // TODO(satorux): Remove this. crbug.com/139649
  void TakeOverEntries(DriveDirectory* dir);

  // Takes over entry represented by |resource_id|. Helper function for
  // TakeOverEntries. TODO(satorux): Remove this. crbug.com/139649
  void TakeOverEntry(const std::string& resource_id);

  // Find a child's resource_id by its name. Returns the empty string if not
  // found. TODO(satorux): Remove this. crbug.com/139649
  std::string FindChild(const base::FilePath::StringType& file_name) const;

  // Removes the entry from its children without destroying the
  // entry instance.
  void RemoveChild(DriveEntry* entry);

  // Removes child elements.
  void RemoveChildren();
  void RemoveChildFiles();
  void RemoveChildDirectories();

  // Recursively extracts the paths set of all sub-directories.
  void GetChildDirectoryPaths(std::set<base::FilePath>* child_dirs);

  // Map between base_name and resource_id of files and directories.
  typedef std::map<base::FilePath::StringType, std::string> ChildMap;
  // Collection of children.
  ChildMap children_;

  DISALLOW_COPY_AND_ASSIGN(DriveDirectory);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_
