// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

class DriveDirectory;

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

  // Sets |base_name_| based on the value of |title_| without name
  // de-duplication (see AddEntry() for details on de-duplication).
  virtual void SetBaseNameFromTitle();

 protected:
  friend class DriveResourceMetadata;  // For access to ctor.
  // For access to set_parent_resource_id() from AddEntry.
  friend class DriveDirectory;

  DriveEntry();

  // Sets the parent directory of this file system entry.
  // It is intended to be used by DriveDirectory::AddEntry() only.
  void set_parent_resource_id(const std::string& parent_resource_id);

  DriveEntryProto proto_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveEntry);
};

// Represents "directory" in a drive virtual file system. Maps to drive
// collection element.
class DriveDirectory : public DriveEntry {
 public:
  virtual ~DriveDirectory();

  // Returns the changestamp of this directory. See drive.proto for details.
  int64 changestamp() const;
  void set_changestamp(int64 changestamp);

 private:
  // TODO(satorux): Remove the friend statements. crbug.com/139649
  friend class DriveResourceMetadata;

  DriveDirectory();
  virtual DriveDirectory* AsDriveDirectory() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DriveDirectory);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILES_H_
