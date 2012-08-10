// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
class SequencedTaskRunner;
}

namespace gdata {

struct CreateDBParams;
class GDataFile;
class GDataDirectory;
class GDataDirectoryService;
class ResourceMetadataDB;

class GDataEntryProto;
class GDataDirectoryProto;
class GDataRootDirectoryProto;
class PlatformFileInfoProto;

// The root directory content origin.
enum ContentOrigin {
  UNINITIALIZED,
  // Content is currently loading from somewhere.  Needs to wait.
  INITIALIZING,
  // Content is initialized, but during refreshing.
  REFRESHING,
  // Content is initialized from disk cache.
  FROM_CACHE,
  // Content is initialized from the direct server response.
  FROM_SERVER,
};

// File type on the gdata file system can be either a regular file or
// a hosted document.
enum GDataFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

// The root directory name used for the Google Drive file system tree. The
// name is used in URLs for the file manager, hence user-visible.
const FilePath::CharType kGDataRootDirectory[] = FILE_PATH_LITERAL("drive");

// The resource ID for the root directory is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
const char kGDataRootDirectoryResourceId[] = "folder:root";

// This should be incremented when incompatibility change is made in
// gdata.proto.
const int32 kProtoVersion = 2;

// Used for file operations like removing files.
typedef base::Callback<void(GDataFileError error)>
    FileOperationCallback;

// Callback similar to FileOperationCallback but with a given |file_path|.
// Used for operations that change a file path like moving files.
typedef base::Callback<void(GDataFileError error,
                            const FilePath& file_path)>
    FileMoveCallback;

// Used to get entry info from the file system.
// If |error| is not GDATA_FILE_OK, |entry_info| is set to NULL.
typedef base::Callback<void(GDataFileError error,
                            scoped_ptr<GDataEntryProto> entry_proto)>
    GetEntryInfoCallback;

// Used to read a directory from the file system.
// If |error| is not GDATA_FILE_OK, |entries| is set to NULL.
// |entries| are contents, both files and directories, of the directory.
typedef std::vector<GDataEntryProto> GDataEntryProtoVector;
typedef base::Callback<void(GDataFileError error,
                            scoped_ptr<GDataEntryProtoVector> entries)>
    ReadDirectoryCallback;

// This is a part of EntryInfoPairResult.
struct EntryInfoResult {
  EntryInfoResult();
  ~EntryInfoResult();

  FilePath path;
  GDataFileError error;
  scoped_ptr<GDataEntryProto> proto;
};

// The result of GetEntryInfoPairCallback(). Used to get a pair of entries
// in one function call.
struct EntryInfoPairResult {
  EntryInfoPairResult();
  ~EntryInfoPairResult();

  EntryInfoResult first;
  EntryInfoResult second;  // Only filled if the first entry is found.
};

// Used to receive the result from GetEntryInfoPairCallback().
typedef base::Callback<void(scoped_ptr<EntryInfoPairResult> pair_result)>
    GetEntryInfoPairCallback;

// Base class for representing files and directories in gdata virtual file
// system.
class GDataEntry {
 public:
  virtual ~GDataEntry();

  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();

  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(DocumentEntry* doc);

  // const versions of AsGDataFile and AsGDataDirectory.
  const GDataFile* AsGDataFileConst() const;
  const GDataDirectory* AsGDataDirectoryConst() const;

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
  bool FromProto(const GDataEntryProto& proto) WARN_UNUSED_RESULT;
  void ToProto(GDataEntryProto* proto) const;

  // Similar to ToProto() but this fills in |file_specific_info| and
  // |directory_specific_info| based on the actual type of the entry.
  // Used to obtain full metadata of a file or a directory as
  // GDataEntryProto.
  void ToProtoFull(GDataEntryProto* proto) const;

  // Escapes forward slashes from file names with magic unicode character
  // \u2215 pretty much looks the same in UI.
  static std::string EscapeUtf8FileName(const std::string& input);

  // Unescapes what was escaped in EScapeUtf8FileName.
  static std::string UnescapeUtf8FileName(const std::string& input);

  // Return the parent of this entry. NULL for root.
  GDataDirectory* parent() const { return parent_; }
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

  // Upload URL is used for uploading files. See gdata.proto for details.
  const GURL& upload_url() const { return upload_url_; }
  void set_upload_url(const GURL& url) { upload_url_ = url; }

  // The edit URL is used for removing files and hosted documents.
  const GURL& edit_url() const { return edit_url_; }

  // The resource id of the parent folder. This piece of information is needed
  // to pair files from change feeds with their directory parents withing the
  // existing file system snapshot (GDataDirectoryService::resource_map_).
  const std::string& parent_resource_id() const { return parent_resource_id_; }

  // True if file was deleted. Used only for instances that are generated from
  // delta feeds.
  bool is_deleted() const { return deleted_; }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of GDataFileSyste
  // class.
  FilePath GetFilePath() const;

  // Sets |base_name_| based on the value of |title_| without name
  // de-duplication (see AddEntry() for details on de-duplication).
  virtual void SetBaseNameFromTitle();

 protected:
  // For access to SetParent from AddEntry.
  friend class GDataDirectory;

  explicit GDataEntry(GDataDirectoryService* directory_service);

  // Sets the parent directory of this file system entry.
  // It is intended to be used by GDataDirectory::AddEntry() only.
  void SetParent(GDataDirectory* parent);

  base::PlatformFileInfo file_info_;
  // Title of this file (i.e. the 'title' attribute associated with a regular
  // file, hosted document, or collection). The title is used to derive
  // |base_name_| but may be different from |base_name_|. For example,
  // |base_name_| has an added .g<something> extension for hosted documents or
  // may have an extra suffix for name de-duplication on the gdata file system.
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

  // Name of this file in the gdata virtual file system. This can change
  // due to de-duplication (See AddEntry).
  FilePath::StringType base_name_;

  GDataDirectory* parent_;
  // Weak pointer to GDataDirectoryService.
  GDataDirectoryService* directory_service_;
  bool deleted_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataEntry);
};

typedef std::map<FilePath::StringType, GDataFile*> GDataFileCollection;
typedef std::map<FilePath::StringType, GDataDirectory*>
    GDataDirectoryCollection;

// Represents "file" in in a GData virtual file system. On gdata feed side,
// this could be either a regular file or a server side document.
class GDataFile : public GDataEntry {
 public:
  virtual ~GDataFile();

  // Converts to/from proto.
  bool FromProto(const GDataEntryProto& proto) WARN_UNUSED_RESULT;
  void ToProto(GDataEntryProto* proto) const;

  DocumentEntry::EntryKind kind() const { return kind_; }
  const GURL& thumbnail_url() const { return thumbnail_url_; }
  const GURL& alternate_url() const { return alternate_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& file_md5() const { return file_md5_; }
  void set_file_md5(const std::string& file_md5) { file_md5_ = file_md5; }
  const std::string& document_extension() const { return document_extension_; }
  bool is_hosted_document() const { return is_hosted_document_; }
  void set_file_info(const base::PlatformFileInfo& info) { file_info_ = info; }

  // Overrides GDataEntry::SetBaseNameFromTitle() to set |base_name_| based
  // on the value of |title_| as well as |is_hosted_document_| and
  // |document_extension_| for hosted documents.
  virtual void SetBaseNameFromTitle() OVERRIDE;

 private:
  friend class GDataDirectoryService;  // For access to ctor.

  explicit GDataFile(GDataDirectoryService* directory_service);
  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(DocumentEntry* doc) OVERRIDE;

  virtual GDataFile* AsGDataFile() OVERRIDE;

  DocumentEntry::EntryKind kind_;  // Not saved in proto.
  GURL thumbnail_url_;
  GURL alternate_url_;
  std::string content_mime_type_;
  std::string file_md5_;
  std::string document_extension_;
  bool is_hosted_document_;

  DISALLOW_COPY_AND_ASSIGN(GDataFile);
};

// Represents "directory" in a GData virtual file system. Maps to gdata
// collection element.
class GDataDirectory : public GDataEntry {
 public:
  virtual ~GDataDirectory();

  // Converts to/from proto.
  bool FromProto(const GDataDirectoryProto& proto) WARN_UNUSED_RESULT;
  void ToProto(GDataDirectoryProto* proto) const;

  // Converts the children as a vector of GDataEntryProto.
  scoped_ptr<GDataEntryProtoVector> ToProtoVector() const;

  // Collection of children files/directories.
  const GDataFileCollection& child_files() const { return child_files_; }
  const GDataDirectoryCollection& child_directories() const {
    return child_directories_;
  }

 private:
  // TODO(satorux): Remove the friend statements. crbug.com/139649
  friend class GDataDirectoryService;
  friend class GDataFileSystem;
  friend class GDataWapiFeedProcessor;

  explicit GDataDirectory(GDataDirectoryService* directory_service);

  // Initializes from DocumentEntry.
  virtual void InitFromDocumentEntry(DocumentEntry* doc) OVERRIDE;

  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name de-duplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  // TODO(satorux): Remove this. crbug.com/139649
  void AddEntry(GDataEntry* entry);

  // Removes the entry from its children list and destroys the entry instance.
  // TODO(satorux): Remove this. crbug.com/139649
  void RemoveEntry(GDataEntry* entry);

  // Takes over all entries from |dir|.
  // TODO(satorux): Remove this. crbug.com/139649
  bool TakeOverEntries(GDataDirectory* dir);

  // Find a child by its name.
  // TODO(satorux): Remove this. crbug.com/139649
  GDataEntry* FindChild(const FilePath::StringType& file_name) const;

  // Add |entry| to children.
  void AddChild(GDataEntry* entry);

  // Removes the entry from its children without destroying the
  // entry instance.
  void RemoveChild(GDataEntry* entry);

  // Removes child elements.
  void RemoveChildren();
  void RemoveChildFiles();
  void RemoveChildDirectories();

  // Collection of children GDataEntry items.
  GDataFileCollection child_files_;
  GDataDirectoryCollection child_directories_;

  DISALLOW_COPY_AND_ASSIGN(GDataDirectory);
};

// TODO(achuith,hashimoto,satorux): Move this to a separate file.
// crbug.com/140317.
// Class to handle GDataEntry* lookups, add/remove GDataEntry*.
class GDataDirectoryService {
 public:
  // Callback for GetEntryByResourceIdAsync.
  typedef base::Callback<void(GDataEntry* entry)> GetEntryByResourceIdCallback;

  // Map of resource id and serialized GDataEntry.
  typedef std::map<std::string, std::string> SerializedMap;

  GDataDirectoryService();
  ~GDataDirectoryService();

  GDataDirectory* root() { return root_.get(); }

  // Last time when we dumped serialized file system to disk.
  const base::Time& last_serialized() const { return last_serialized_; }
  void set_last_serialized(const base::Time& time) { last_serialized_ = time; }
  // Size of serialized file system on disk in bytes.
  const size_t serialized_size() const { return serialized_size_; }
  void set_serialized_size(size_t size) { serialized_size_ = size; }

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  const int64 largest_changestamp() const { return largest_changestamp_; }
  void set_largest_changestamp(int64 value) { largest_changestamp_ = value; }

  // The root directory content origin.
  const ContentOrigin origin() const { return origin_; }
  void set_origin(ContentOrigin value) { origin_ = value; }

  // Creates a GDataEntry from a DocumentEntry.
  GDataEntry* FromDocumentEntry(DocumentEntry* doc);

  // Creates a GDataFile instance.
  GDataFile* CreateGDataFile();

  // Creates a GDataDirectory instance.
  GDataDirectory* CreateGDataDirectory();

  // Sets root directory resource id and initialize the root entry.
  void InitializeRootEntry(const std::string& root_id);

  // Move |entry| to |directory_path| asynchronously. Removes entry from
  // previous parent. Must be called on UI thread. |callback| is called on the
  // UI thread.
  void MoveEntryToDirectory(const FilePath& directory_path,
                            GDataEntry* entry,
                            const FileMoveCallback& callback);

  // Adds the entry to resource map.
  void AddEntryToResourceMap(GDataEntry* entry);

  // Removes the entry from resource map.
  void RemoveEntryFromResourceMap(GDataEntry* entry);

  // Searches for |file_path| synchronously.
  // TODO(satorux): Replace this with an async version crbug.com/137160
  GDataEntry* FindEntryByPathSync(const FilePath& file_path);

  // Searches for |file_path| synchronously, and runs |callback|.
  // TODO(satorux): Replace this with an async version crbug.com/137160
  void FindEntryByPathAndRunSync(const FilePath& file_path,
                                 const FindEntryCallback& callback);

  // Returns the GDataEntry* with the corresponding |resource_id|.
  // TODO(achuith): Get rid of this in favor of async version crbug.com/13957.
  GDataEntry* GetEntryByResourceId(const std::string& resource_id);

  // Returns the GDataEntry* in the callback with the corresponding
  // |resource_id|. TODO(achuith): Rename this to GetEntryByResourceId.
  void GetEntryByResourceIdAsync(const std::string& resource_id,
                                 const GetEntryByResourceIdCallback& callback);

  // Finds an entry (a file or a directory) by |file_path|.
  //
  // Must be called from UI thread. |callback| is run on UI thread.
  void GetEntryInfoByPath(const FilePath& file_path,
                          const GetEntryInfoCallback& callback);

  // Finds and reads a directory by |file_path|.
  //
  // Must be called from UI thread. |callback| is run on UI thread.
  void ReadDirectoryByPath(const FilePath& file_path,
                           const ReadDirectoryCallback& callback);

  // Similar to GetEntryInfoByPath() but this function finds a pair of
  // entries by |first_path| and |second_path|. If the entry for
  // |first_path| is not found, this function does not try to get the
  // entry of |second_path|.
  //
  // Must be called from UI thread. |callback| is run on UI thread.
  void GetEntryInfoPairByPaths(
      const FilePath& first_path,
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback);

  // Replaces file entry with the same resource id as |fresh_file| with its
  // fresh value |fresh_file|.
  void RefreshFile(scoped_ptr<GDataFile> fresh_file);

  // Replaces file entry |old_entry| with its fresh value |fresh_file|.
  static void RefreshFileInternal(scoped_ptr<GDataFile> fresh_file,
                                  GDataEntry* old_entry);

  // Serializes/Parses to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto) const;
  bool ParseFromString(const std::string& serialized_proto);

  // Restores from and saves to database.
  void InitFromDB(const FilePath& db_path,
                  base::SequencedTaskRunner* blocking_task_runner,
                  const FileOperationCallback& callback);
  void SaveToDB();

 private:
  // A map table of file's resource string to its GDataFile* entry.
  typedef std::map<std::string, GDataEntry*> ResourceMap;

  // Initializes the resource map using serialized_resources fetched from the
  // database.
  void InitResourceMap(CreateDBParams* create_params,
                       const FileOperationCallback& callback);

  // Clears root_ and the resource map.
  void ClearRoot();

  // Creates GDataEntry from serialized string.
  scoped_ptr<GDataEntry> FromProtoString(
      const std::string& serialized_proto);

  // ...
  void GetEntryInfoPairByPathsAfterGetFirst(
      const FilePath& first_path,
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // ...
  void GetEntryInfoPairByPathsAfterGetSecond(
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      scoped_ptr<EntryInfoPairResult> result,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // Private data members.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<ResourceMetadataDB> directory_service_db_;

  ResourceMap resource_map_;

  scoped_ptr<GDataDirectory> root_;  // Stored in the serialized proto.

  base::Time last_serialized_;
  size_t serialized_size_;
  int64 largest_changestamp_;  // Stored in the serialized proto.
  ContentOrigin origin_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<GDataDirectoryService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataDirectoryService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
