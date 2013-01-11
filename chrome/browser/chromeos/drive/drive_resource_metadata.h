// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"

namespace base {
class SequencedTaskRunner;
}

namespace google_apis {
class ResourceEntry;
}

namespace drive {

struct CreateDBParams;
class DriveDirectory;
class DriveEntry;
class DriveEntryProto;
class DriveFile;
class ResourceMetadataDB;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;
typedef std::map<std::string /* resource_id */, DriveEntryProto>
    DriveEntryProtoMap;

// File type on the drive file system can be either a regular file or
// a hosted document.
enum DriveFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

// The root directory name used for the Google Drive file system tree. The
// name is used in URLs for the file manager, hence user-visible.
const FilePath::CharType kDriveRootDirectory[] = FILE_PATH_LITERAL("drive");

// The resource ID for the root directory for WAPI is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
// Note that this special ID only applies to WAPI. Drive uses a non-constant
// unique ID given in About resource.
const char kWAPIRootDirectoryResourceId[] = "folder:root";

// This should be incremented when incompatibility change is made in
// drive.proto.
const int32 kProtoVersion = 2;

// Callback similar to FileOperationCallback but with a given |file_path|.
// Used for operations that change a file path like moving files.
typedef base::Callback<void(DriveFileError error,
                            const FilePath& file_path)>
    FileMoveCallback;

// Used to get entry info from the file system.
// If |error| is not DRIVE_FILE_OK, |entry_info| is set to NULL.
typedef base::Callback<void(DriveFileError error,
                            scoped_ptr<DriveEntryProto> entry_proto)>
    GetEntryInfoCallback;

typedef base::Callback<void(DriveFileError error,
                            scoped_ptr<DriveEntryProtoVector> entries)>
    ReadDirectoryCallback;

// Used to get entry info from the file system, with the Drive file path.
// If |error| is not DRIVE_FILE_OK, |entry_proto| is set to NULL.
//
// |drive_file_path| parameter is provided as DriveEntryProto does not contain
// the Drive file path (i.e. only contains the base name without parent
// directory names).
typedef base::Callback<void(DriveFileError error,
                            const FilePath& drive_file_path,
                            scoped_ptr<DriveEntryProto> entry_proto)>
    GetEntryInfoWithFilePathCallback;

// Used to get a set of changed directories for feed processing.
typedef base::Callback<void(const std::set<FilePath>&)>
    GetChildDirectoriesCallback;

// This is a part of EntryInfoPairResult.
struct EntryInfoResult {
  EntryInfoResult();
  ~EntryInfoResult();

  FilePath path;
  DriveFileError error;
  scoped_ptr<DriveEntryProto> proto;
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

// Storage for Drive Metadata.
class DriveResourceMetadata {
 public:
  // Map of resource id and serialized DriveEntry.
  typedef std::map<std::string, std::string> SerializedMap;
  // Map of resource id strings to DriveEntry*.
  typedef std::map<std::string, DriveEntry*> ResourceMap;

  DriveResourceMetadata();
  ~DriveResourceMetadata();

  // Last time when we dumped serialized file system to disk.
  const base::Time& last_serialized() const { return last_serialized_; }
  void set_last_serialized(const base::Time& time) { last_serialized_ = time; }
  // Size of serialized file system on disk in bytes.
  size_t serialized_size() const { return serialized_size_; }
  void set_serialized_size(size_t size) { serialized_size_ = size; }

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  int64 largest_changestamp() const { return largest_changestamp_; }
  void set_largest_changestamp(int64 value) { largest_changestamp_ = value; }

  // True if the file system feed is loaded from the cache or from the server.
  bool loaded() const { return loaded_; }
  void set_loaded(bool loaded) { loaded_ = loaded; }

  // Add |entry| to directory with path |directory_path| and invoke the
  // callback asynchronously.
  // |callback| must not be null.
  void AddEntryToDirectory(const FilePath& directory_path,
                           scoped_ptr<google_apis::ResourceEntry> entry,
                           const FileMoveCallback& callback);

  // Add |entry_proto| to the metadata tree, based on its parent_resource_id.
  // |callback| must not be null.
  void AddEntryToParent(const DriveEntryProto& entry_proto,
                        const FileMoveCallback& callback);

  // Moves entry specified by |file_path| to the directory specified by
  // |directory_path| and calls the callback asynchronously. Removes the entry
  // from the previous parent.
  // |callback| must not be null.
  void MoveEntryToDirectory(const FilePath& file_path,
                            const FilePath& directory_path,
                            const FileMoveCallback& callback);

  // Renames entry specified by |file_path| with the new name |new_name| and
  // calls |callback| asynchronously.
  // |callback| must not be null.
  void RenameEntry(const FilePath& file_path,
                   const FilePath::StringType& new_name,
                   const FileMoveCallback& callback);

  // Removes entry with |resource_id| from its parent. Calls |callback| with the
  // path of the parent directory. |callback| must not be null.
  void RemoveEntryFromParent(const std::string& resource_id,
                             const FileMoveCallback& callback);

  // Finds an entry (a file or a directory) by |resource_id|.
  // |callback| must not be null.
  void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback);

  // Finds an entry (a file or a directory) by |file_path|.
  // |callback| must not be null.
  void GetEntryInfoByPath(const FilePath& file_path,
                          const GetEntryInfoCallback& callback);

  // Finds and reads a directory by |file_path|.
  // |callback| must not be null.
  void ReadDirectoryByPath(const FilePath& file_path,
                           const ReadDirectoryCallback& callback);

  // Similar to GetEntryInfoByPath() but this function finds a pair of
  // entries by |first_path| and |second_path|. If the entry for
  // |first_path| is not found, this function does not try to get the
  // entry of |second_path|. |callback| must not be null.
  void GetEntryInfoPairByPaths(
      const FilePath& first_path,
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback);

  // Refreshes a drive entry with the same resource id as |entry_proto|.
  // |callback| is run with the error, file path and proto of the entry.
  // |callback| must not be null.
  void RefreshEntry(const DriveEntryProto& entry_proto,
                    const GetEntryInfoWithFilePathCallback& callback);

  // Removes all child files of |directory| and replaces them with
  // |entry_proto_map|. |callback| is called with the directory path.
  // |callback| must not be null.
  void RefreshDirectory(const std::string& directory_resource_id,
                        const DriveEntryProtoMap& entry_proto_map,
                        const FileMoveCallback& callback);

  // Recursively get child directories of entry pointed to by |resource_id|.
  void GetChildDirectories(
      const std::string& resource_id,
      const GetChildDirectoriesCallback& changed_dirs_callback);

  // Removes all files/directories under root (not including root).
  void RemoveAll(const base::Closure& callback);

  // Serializes/Parses to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto) const;
  bool ParseFromString(const std::string& serialized_proto);

  // Restores from and saves to database, calling |callback| asynchronously.
  // |callback| must not be null.
  void InitFromDB(const FilePath& db_path,
                  base::SequencedTaskRunner* blocking_task_runner,
                  const FileOperationCallback& callback);
  void SaveToDB();

  // TODO(achuith): Remove all DriveEntry based methods. crbug.com/127856.
  // Creates DriveEntry from proto.
  scoped_ptr<DriveEntry> CreateDriveEntryFromProto(
      const DriveEntryProto& entry_proto);

  // Creates a DriveFile instance.
  scoped_ptr<DriveFile> CreateDriveFile();

  // Creates a DriveDirectory instance.
  scoped_ptr<DriveDirectory> CreateDriveDirectory();

  // Adds the entry to resource map. Returns false if an entry with the same
  // resource_id exists.
  bool AddEntryToResourceMap(DriveEntry* entry);

  // Removes the entry from resource map.
  void RemoveEntryFromResourceMap(const std::string& resource_id);

  // Returns the DriveEntry* with the corresponding |resource_id|.
  // TODO(satorux): Remove this in favor of GetEntryInfoByResourceId()
  // but can be difficult. See crbug.com/137374
  DriveEntry* GetEntryByResourceId(const std::string& resource_id);

 private:
  // Initializes the resource map using serialized_resources fetched from the
  // database.
  // |callback| must not be null.
  void InitResourceMap(CreateDBParams* create_params,
                       const FileOperationCallback& callback);

  // Clears root_ and the resource map.
  void ClearRoot();

  // Creates DriveEntry from serialized string.
  scoped_ptr<DriveEntry> CreateDriveEntryFromProtoString(
      const std::string& serialized_proto);

  // Continues with GetEntryInfoPairByPaths after the first DriveEntry has been
  // asynchronously fetched. This fetches the second DriveEntry only if the
  // first was found.
  void GetEntryInfoPairByPathsAfterGetFirst(
      const FilePath& first_path,
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Continues with GetIntroInfoPairByPaths after the second DriveEntry has been
  // asynchronously fetched.
  void GetEntryInfoPairByPathsAfterGetSecond(
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      scoped_ptr<EntryInfoPairResult> result,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Searches for |file_path| synchronously.
  DriveEntry* FindEntryByPathSync(const FilePath& file_path);

  // Helper function to add |entry_proto| as a child to |directory|.
  // |callback| must not be null.
  void AddEntryToDirectoryInternal(DriveDirectory* directory,
                                   const DriveEntryProto& entry_proto,
                                   const FileMoveCallback& callback);

  // Helper function to get a parent directory given |parent_resource_id|.
  // Returns root if |parent_resource_id| is empty. Returns NULL if
  // |parent_resource_id| is not empty and the corresponding entry is not a
  // directory.
  DriveDirectory* GetParent(const std::string& parent_resource_id);

  // Private data members.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<ResourceMetadataDB> resource_metadata_db_;

  ResourceMap resource_map_;

  scoped_ptr<DriveDirectory> root_;  // Stored in the serialized proto.

  base::Time last_serialized_;
  size_t serialized_size_;
  int64 largest_changestamp_;  // Stored in the serialized proto.
  bool loaded_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveResourceMetadata> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveResourceMetadata);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_
