// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DIRECTORY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DIRECTORY_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"

namespace base {
class SequencedTaskRunner;
}

namespace gdata {

struct CreateDBParams;
class DocumentEntry;
class GDataEntry;
class GDataEntryProto;
class GDataFile;
class GDataDirectory;
class ResourceMetadataDB;

typedef std::vector<GDataEntryProto> GDataEntryProtoVector;

// File type on the gdata file system can be either a regular file or
// a hosted document.
enum GDataFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

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

// The root directory name used for the Google Drive file system tree. The
// name is used in URLs for the file manager, hence user-visible.
const FilePath::CharType kGDataRootDirectory[] = FILE_PATH_LITERAL("drive");

// The resource ID for the root directory is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
const char kGDataRootDirectoryResourceId[] = "folder:root";

// This should be incremented when incompatibility change is made in
// gdata.proto.
const int32 kProtoVersion = 2;

// Callback type used to get result of file search.
// If |error| is not PLATFORM_FILE_OK, |entry| is set to NULL.
typedef base::Callback<void(GDataFileError error, GDataEntry* entry)>
    FindEntryCallback;

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

// Class to handle GDataEntry* lookups, add/remove GDataEntry*.
class GDataDirectoryService {
 public:
  // Callback for GetEntryByResourceIdAsync.
  typedef base::Callback<void(GDataEntry* entry)> GetEntryByResourceIdCallback;

  // Map of resource id and serialized GDataEntry.
  typedef std::map<std::string, std::string> SerializedMap;
  // Map of resource id strings to GDataEntry*.
  typedef std::map<std::string, GDataEntry*> ResourceMap;

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

  // Add |new entry| to |directory| and invoke the callback asynchronously.
  // |callback| may not be null.
  // TODO(achuith,satorux): Use GDataEntryProto instead for new_entry.
  // crbug.com/142048
  void AddEntryToDirectory(GDataDirectory* directory,
                           GDataEntry* new_entry,
                           const FileMoveCallback& callback);

  // Moves |entry| to |directory_path| asynchronously. Removes entry from
  // previous parent. Must be called on UI thread. |callback| is called on the
  // UI thread. |callback| may not be null.
  void MoveEntryToDirectory(const FilePath& directory_path,
                            GDataEntry* entry,
                            const FileMoveCallback& callback);

  // Removes |entry| from its parent. Calls |callback| with the path of the
  // parent directory. |callback| may not be null.
  void RemoveEntryFromParent(GDataEntry* entry,
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

  // Removes all child files of |directory| and replace with file_map.
  // |callback| is called with the directory path. |callback| may not be null.
  void RefreshDirectory(const std::string& directory_resource_id,
                        const ResourceMap& file_map,
                        const FileMoveCallback& callback);

  // Serializes/Parses to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto) const;
  bool ParseFromString(const std::string& serialized_proto);

  // Restores from and saves to database.
  void InitFromDB(const FilePath& db_path,
                  base::SequencedTaskRunner* blocking_task_runner,
                  const FileOperationCallback& callback);
  void SaveToDB();

 private:
  // Initializes the resource map using serialized_resources fetched from the
  // database.
  void InitResourceMap(CreateDBParams* create_params,
                       const FileOperationCallback& callback);

  // Clears root_ and the resource map.
  void ClearRoot();

  // Creates GDataEntry from serialized string.
  scoped_ptr<GDataEntry> FromProtoString(
      const std::string& serialized_proto);

  // Continues with GetEntryInfoPairByPaths after the first GDataEntry has been
  // asynchronously fetched. This fetches the second GDataEntry only if the
  // first was found.
  void GetEntryInfoPairByPathsAfterGetFirst(
      const FilePath& first_path,
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // Continues with GetIntroInfoPairByPaths after the second GDataEntry has been
  // asynchronously fetched.
  void GetEntryInfoPairByPathsAfterGetSecond(
      const FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      scoped_ptr<EntryInfoPairResult> result,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // These internal functions need friend access to private GDataDirectory
  // methods.
  // Replaces file entry |old_entry| with its fresh value |fresh_file|.
  static void RefreshFileInternal(scoped_ptr<GDataFile> fresh_file,
                                  GDataEntry* old_entry);

  // Removes all child files of |directory| and replace with file_map.
  // |callback| may not be null.
  static void RefreshDirectoryInternal(const ResourceMap& file_map,
                                       const FileMoveCallback& callback,
                                       GDataEntry* directory_entry);


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

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DIRECTORY_SERVICE_H_

