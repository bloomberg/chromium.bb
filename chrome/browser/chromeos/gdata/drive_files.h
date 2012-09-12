// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILES_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILES_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"

namespace gdata {

class DriveDirectory;
class DriveDirectoryProto;
class DriveEntryProto;
class DriveFile;
class DriveResourceMetadata;
class PlatformFileInfoProto;

// Used to read a directory from the file system.
// If |error| is not DRIVE_FILE_OK, |entries| is set to NULL.
// |entries| are contents, both files and directories, of the directory.
typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

// Base class for representing files and directories in Drive virtual file
// system.
class DriveEntry {
 public:
  virtual ~DriveEntry();

  virtual DriveFile* AsDriveFile();
  virtual DriveDirectory* AsDriveDirectory();

  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(const DocumentEntry& doc);

  // const versions of AsDriveFile and AsDriveDirectory.
  const DriveFile* AsDriveFileConst() const;
  const DriveDirectory* AsDriveDirectoryConst() const;

  // Serialize/Parse to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto) const;

  // Converts the proto representation to the platform file.
  static void ConvertProtoToPlatformFileInfo(
      const PlatformFileInfoProto& proto,
      base::PlatformFileInfo* file_info);

  // Converts the platform file info to the proto representation.
  static void ConvertPlatformFileInfoToProto(
      const base::PlatformFileInfo& file_info,
      PlatformFileInfoProto* proto);

  // Converts to/from proto. Only handles the common part (i.e. does not
  // touch |file_specific_info|).
  void FromProto(const DriveEntryProto& proto);
  // TODO(achuith,satorux): Should this be virtual?
  void ToProto(DriveEntryProto* proto) const;

  // Similar to ToProto() but this fills in |file_specific_info| and
  // |directory_specific_info| based on the actual type of the entry.
  // Used to obtain full metadata of a file or a directory as
  // DriveEntryProto.
  void ToProtoFull(DriveEntryProto* proto) const;

  // Return the parent of this entry. NULL for root.
  DriveDirectory* parent() const { return parent_; }
  const base::PlatformFileInfo& file_info() const { return file_info_; }

  // This is not the full path, use GetFilePath for that.
  // Note that base_name_ gets reset by SetBaseNameFromTitle() in a number of
  // situations due to de-duplication (see AddEntry).
  const FilePath::StringType& base_name() const { return base_name_; }
  // TODO(achuith): Make this private when GDataDB no longer uses path as a key.
  void set_base_name(const FilePath::StringType& name) { base_name_ = name; }

  const FilePath::StringType& title() const { return title_; }
  void set_title(const FilePath::StringType& title) { title_ = title; }

  // The unique resource ID associated with this file system entry.
  const std::string& resource_id() const { return resource_id_; }
  void set_resource_id(const std::string& res_id) { resource_id_ = res_id; }

  // The content URL is used for downloading regular files as is.
  const GURL& content_url() const { return content_url_; }
  void set_content_url(const GURL& url) { content_url_ = url; }

  // Upload URL is used for uploading files. See drive.proto for details.
  const GURL& upload_url() const { return upload_url_; }
  void set_upload_url(const GURL& url) { upload_url_ = url; }

  // The edit URL is used for removing files and hosted documents.
  const GURL& edit_url() const { return edit_url_; }

  // The resource id of the parent folder. This piece of information is needed
  // to pair files from change feeds with their directory parents withing the
  // existing file system snapshot (DriveResourceMetadata::resource_map_).
  const std::string& parent_resource_id() const { return parent_resource_id_; }

  // True if file was deleted. Used only for instances that are generated from
  // delta feeds.
  bool is_deleted() const { return deleted_; }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of DriveFileSystem
  // class.
  FilePath GetFilePath() const;

  // Sets |base_name_| based on the value of |title_| without name
  // de-duplication (see AddEntry() for details on de-duplication).
  virtual void SetBaseNameFromTitle();

 protected:
  // For access to SetParent from AddEntry.
  friend class DriveDirectory;

  explicit DriveEntry(DriveResourceMetadata* resource_metadata);

  // Sets the parent directory of this file system entry.
  // It is intended to be used by DriveDirectory::AddEntry() only.
  void SetParent(DriveDirectory* parent);

  base::PlatformFileInfo file_info_;
  // Title of this file (i.e. the 'title' attribute associated with a regular
  // file, hosted document, or collection). The title is used to derive
  // |base_name_| but may be different from |base_name_|. For example,
  // |base_name_| has an added .g<something> extension for hosted documents or
  // may have an extra suffix for name de-duplication on the drive file system.
  FilePath::StringType title_;
  std::string resource_id_;
  std::string parent_resource_id_;
  // Files with the same title will be uniquely identified with this field
  // so we can represent them with unique URLs/paths in File API layer.
  // For example, two files in the same directory with the same name "Foo"
  // will show up in the virtual directory as "Foo" and "Foo (2)".
  GURL edit_url_;
  GURL content_url_;
  GURL upload_url_;

  // Remaining fields are not serialized.

  // Name of this file in the drive virtual file system. This can change
  // due to de-duplication (See AddEntry).
  FilePath::StringType base_name_;

  DriveDirectory* parent_;
  // Weak pointer to DriveResourceMetadata.
  DriveResourceMetadata* resource_metadata_;
  bool deleted_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveEntry);
};

// Represents "file" in in a drive virtual file system. On gdata feed side,
// this could be either a regular file or a server side document.
class DriveFile : public DriveEntry {
 public:
  virtual ~DriveFile();

  // Converts to/from proto.
  void FromProto(const DriveEntryProto& proto);
  void ToProto(DriveEntryProto* proto) const;

  DriveEntryKind kind() const { return kind_; }
  const GURL& thumbnail_url() const { return thumbnail_url_; }
  const GURL& alternate_url() const { return alternate_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& file_md5() const { return file_md5_; }
  void set_file_md5(const std::string& file_md5) { file_md5_ = file_md5; }
  const std::string& document_extension() const { return document_extension_; }
  bool is_hosted_document() const { return is_hosted_document_; }
  void set_file_info(const base::PlatformFileInfo& info) { file_info_ = info; }

  // Overrides DriveEntry::SetBaseNameFromTitle() to set |base_name_| based
  // on the value of |title_| as well as |is_hosted_document_| and
  // |document_extension_| for hosted documents.
  virtual void SetBaseNameFromTitle() OVERRIDE;

 private:
  friend class DriveResourceMetadata;  // For access to ctor.

  explicit DriveFile(DriveResourceMetadata* resource_metadata);
  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(const DocumentEntry& doc) OVERRIDE;

  virtual DriveFile* AsDriveFile() OVERRIDE;

  DriveEntryKind kind_;  // Not saved in proto.
  GURL thumbnail_url_;
  GURL alternate_url_;
  std::string content_mime_type_;
  std::string file_md5_;
  std::string document_extension_;
  bool is_hosted_document_;

  DISALLOW_COPY_AND_ASSIGN(DriveFile);
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

 private:
  // TODO(satorux): Remove the friend statements. crbug.com/139649
  friend class DriveResourceMetadata;
  friend class DriveResourceMetadataTest;
  friend class GDataWapiFeedProcessor;

  explicit DriveDirectory(DriveResourceMetadata* resource_metadata);

  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(const DocumentEntry& doc) OVERRIDE;

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
  std::string FindChild(const FilePath::StringType& file_name) const;

  // Removes the entry from its children without destroying the
  // entry instance.
  void RemoveChild(DriveEntry* entry);

  // Removes child elements.
  void RemoveChildren();
  void RemoveChildFiles();
  void RemoveChildDirectories();

  // Recursively extracts the paths set of all sub-directories.
  void GetChildDirectoryPaths(std::set<FilePath>* child_dirs);

  // Map between base_name and resource_id of files and directories.
  typedef std::map<FilePath::StringType, std::string> ChildMap;
  // Collection of children.
  ChildMap child_files_;
  ChildMap child_directories_;

  DISALLOW_COPY_AND_ASSIGN(DriveDirectory);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILES_H_
