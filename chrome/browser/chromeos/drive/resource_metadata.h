// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {

class ResourceEntry;
class ResourceMetadataStorage;

typedef std::vector<ResourceEntry> ResourceEntryVector;
typedef std::map<std::string /* resource_id */, ResourceEntry>
    ResourceEntryMap;

// Holds information needed to fetch contents of a directory.
// This object is copyable.
class DirectoryFetchInfo {
 public:
  DirectoryFetchInfo() : changestamp_(0) {}
  DirectoryFetchInfo(const std::string& resource_id,
                     int64 changestamp)
      : resource_id_(resource_id),
        changestamp_(changestamp) {
  }

  // Returns true if the object is empty.
  bool empty() const { return resource_id_.empty(); }

  // Resource ID of the directory.
  const std::string& resource_id() const { return resource_id_; }

  // Changestamp of the directory. The changestamp is used to determine if
  // the directory contents should be fetched.
  int64 changestamp() const { return changestamp_; }

  // Returns a string representation of this object.
  std::string ToString() const;

 private:
  const std::string resource_id_;
  const int64 changestamp_;
};

// Callback similar to FileOperationCallback but with a given |file_path|.
// Used for operations that change a file path like moving files.
typedef base::Callback<void(FileError error,
                            const base::FilePath& file_path)>
    FileMoveCallback;

// Used to get entry info from the file system.
// If |error| is not FILE_ERROR_OK, |entry_info| is set to NULL.
typedef base::Callback<void(FileError error,
                            scoped_ptr<ResourceEntry> entry)>
    GetEntryInfoCallback;

typedef base::Callback<void(FileError error,
                            scoped_ptr<ResourceEntryVector> entries)>
    ReadDirectoryCallback;

// Used to get entry info from the file system, with the Drive file path.
// If |error| is not FILE_ERROR_OK, |entry| is set to NULL.
//
// |drive_file_path| parameter is provided as ResourceEntry does not contain
// the Drive file path (i.e. only contains the base name without parent
// directory names).
typedef base::Callback<void(FileError error,
                            const base::FilePath& drive_file_path,
                            scoped_ptr<ResourceEntry> entry)>
    GetEntryInfoWithFilePathCallback;

// Used to get a set of changed directories for feed processing.
typedef base::Callback<void(const std::set<base::FilePath>&)>
    GetChildDirectoriesCallback;

typedef base::Callback<void(int64)> GetChangestampCallback;

// This is a part of EntryInfoPairResult.
struct EntryInfoResult {
  EntryInfoResult();
  ~EntryInfoResult();

  base::FilePath path;
  FileError error;
  scoped_ptr<ResourceEntry> entry;
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

namespace internal {

// Storage for Drive Metadata.
class ResourceMetadata {
 public:
  // |root_resource_id| is the resource id for the root directory.
  ResourceMetadata(
      const base::FilePath& data_directory_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Initializes this object.
  // This method should be called before any other methods.
  void Initialize(const FileOperationCallback& callback);

  // Destroys this object.  This method posts a task to |blocking_task_runner_|
  // to safely delete this object.
  void Destroy();

  // Resets this object.
  void Reset(const FileOperationCallback& callback);

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  void GetLargestChangestamp(const GetChangestampCallback& callback);
  void SetLargestChangestamp(int64 value,
                             const FileOperationCallback& callback);

  // Adds |entry| to the metadata tree, based on its parent_resource_id.
  // |callback| must not be null.
  void AddEntry(const ResourceEntry& entry,
                const FileMoveCallback& callback);

  // Moves entry specified by |file_path| to the directory specified by
  // |directory_path| and calls the callback asynchronously. Removes the entry
  // from the previous parent. |callback| must not be null.
  void MoveEntryToDirectory(const base::FilePath& file_path,
                            const base::FilePath& directory_path,
                            const FileMoveCallback& callback);

  // Renames entry specified by |file_path| with the new name |new_name| and
  // calls |callback| asynchronously. |callback| must not be null.
  void RenameEntry(const base::FilePath& file_path,
                   const std::string& new_name,
                   const FileMoveCallback& callback);

  // Removes entry with |resource_id| from its parent. Calls |callback| with the
  // path of the parent directory. |callback| must not be null.
  void RemoveEntry(const std::string& resource_id,
                   const FileMoveCallback& callback);

  // Finds an entry (a file or a directory) by |resource_id|.
  // |callback| must not be null.
  void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback);

  // Finds an entry (a file or a directory) by |file_path|.
  // |callback| must not be null.
  void GetEntryInfoByPath(const base::FilePath& file_path,
                          const GetEntryInfoCallback& callback);

  // Finds and reads a directory by |file_path|.
  // |callback| must not be null.
  void ReadDirectoryByPath(const base::FilePath& file_path,
                           const ReadDirectoryCallback& callback);

  // Similar to GetEntryInfoByPath() but this function finds a pair of
  // entries by |first_path| and |second_path|. If the entry for
  // |first_path| is not found, this function does not try to get the
  // entry of |second_path|. |callback| must not be null.
  void GetEntryInfoPairByPaths(const base::FilePath& first_path,
                               const base::FilePath& second_path,
                               const GetEntryInfoPairCallback& callback);

  // Refreshes a drive entry with the same resource id as |entry|.
  // |callback| is run with the error, file path and the new entry.
  // |callback| must not be null.
  void RefreshEntry(const ResourceEntry& entry,
                    const GetEntryInfoWithFilePathCallback& callback);

  // Removes all child files of the directory pointed by
  // |directory_fetch_info| and replaces them with
  // |entry_map|. The changestamp of the directory will be updated per
  // |directory_fetch_info|. |callback| is called with the directory path.
  // |callback| must not be null.
  //
  // TODO(satorux): For "fast fetch" crbug.com/178348, this function should
  // be able to update child directories too. The existing directories should
  // remain as-is, but the new directories should be added with changestamp
  // set to zero, which will be fast fetched.
  void RefreshDirectory(const DirectoryFetchInfo& directory_fetch_info,
                        const ResourceEntryMap& entry_map,
                        const FileMoveCallback& callback);

  // Recursively get child directories of entry pointed to by |resource_id|.
  void GetChildDirectories(
      const std::string& resource_id,
      const GetChildDirectoriesCallback& changed_dirs_callback);

  // Iterates over entries and runs |iterate_callback| for each entry with
  // |blocking_task_runner_|. Runs |completion_callback| after iterating over
  // all entries.
  void IterateEntries(const IterateCallback& iterate_callback,
                      const base::Closure& completion_callback);

 private:
  // Note: Use Destroy() to delete this object.
  ~ResourceMetadata();

  // Used to implement Initialize();
  FileError InitializeOnBlockingPool() WARN_UNUSED_RESULT;

  // Sets up entries which should be present by default.
  bool SetUpDefaultEntries();

  // Used to implement Destroy().
  void DestroyOnBlockingPool();

  // Used to implement Reset().
  FileError ResetOnBlockingPool();

  // Used to implement GetLargestChangestamp().
  int64 GetLargestChangestampOnBlockingPool();

  // Used to implement SetLargestChangestamp().
  FileError SetLargestChangestampOnBlockingPool(int64 value);

  // Used to implement AddEntry().
  FileError AddEntryOnBlockingPool(const ResourceEntry& entry,
                                   base::FilePath* out_file_path);

  // Used to implement MoveEntryToDirectory().
  FileError MoveEntryToDirectoryOnBlockingPool(
      const base::FilePath& file_path,
      const base::FilePath& directory_path,
      base::FilePath* out_file_path);

  // Used to implement RenameEntry().
  FileError RenameEntryOnBlockingPool(const base::FilePath& file_path,
                                      const std::string& new_name,
                                      base::FilePath* out_file_path);

  // Used to implement RemoveEntry().
  FileError RemoveEntryOnBlockingPool(const std::string& resource_id,
                                      base::FilePath* out_file_path);

  // Used to implement GetEntryInfoByResourceId().
  FileError GetEntryInfoByResourceIdOnBlockingPool(
      const std::string& resource_id,
      base::FilePath* out_file_path,
      ResourceEntry* out_entry);

  // Used to implement GetEntryInfoByPath().
  FileError GetEntryInfoByPathOnBlockingPool(const base::FilePath& file_path,
                                             ResourceEntry* out_entry);

  // Used to implement ReadDirectoryByPath().
  FileError ReadDirectoryByPathOnBlockingPool(
      const base::FilePath& file_path,
      ResourceEntryVector* out_entries);

  // Used to implement RefreshEntry().
  FileError RefreshEntryOnBlockingPool(const ResourceEntry& entry,
                                       base::FilePath* out_file_path,
                                       ResourceEntry* out_entry);

  // Used to implement RefreshDirectory().
  FileError RefreshDirectoryOnBlockingPool(
      const DirectoryFetchInfo& directory_fetch_info,
      const ResourceEntryMap& entry_map,
      base::FilePath* out_file_path);

  // Used to implement GetChildDirectories().
  scoped_ptr<std::set<base::FilePath> > GetChildDirectoriesOnBlockingPool(
      const std::string& resource_id);

  // Used to implement IterateEntries().
  void IterateEntriesOnBlockingPool(const IterateCallback& callback);

  // Continues with GetEntryInfoPairByPaths after the first DriveEntry has been
  // asynchronously fetched. This fetches the second DriveEntry only if the
  // first was found.
  void GetEntryInfoPairByPathsAfterGetFirst(
      const base::FilePath& first_path,
      const base::FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Continues with GetIntroInfoPairByPaths after the second DriveEntry has been
  // asynchronously fetched.
  void GetEntryInfoPairByPathsAfterGetSecond(
      const base::FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      scoped_ptr<EntryInfoPairResult> result,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Searches for |file_path| synchronously.
  scoped_ptr<ResourceEntry> FindEntryByPathSync(
      const base::FilePath& file_path);

  // Helper function to get a directory given |resource_id|. |resource_id| can
  // not be empty. Returns NULL if it finds no corresponding entry, or the
  // corresponding entry is not a directory.
  scoped_ptr<ResourceEntry> GetDirectory(const std::string& resource_id);

  // Returns virtual file path of the entry.
  base::FilePath GetFilePath(const std::string& resource_id);

  // Recursively extracts the paths set of all sub-directories.
  void GetDescendantDirectoryPaths(const std::string& resource_id,
                                   std::set<base::FilePath>* child_directories);

  // Puts an entry under its parent directory. Removes the child from the old
  // parent if there is. This method will also do name de-duplication to ensure
  // that the exposed presentation path does not have naming conflicts. Two
  // files with the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  bool PutEntryUnderDirectory(const ResourceEntry& entry);

  // Removes the entry and its descendants.
  bool RemoveEntryRecursively(const std::string& resource_id);

  // Converts the children as a vector of ResourceEntry.
  scoped_ptr<ResourceEntryVector> DirectoryChildrenToProtoVector(
      const std::string& directory_resource_id);

  const base::FilePath data_directory_path_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  scoped_ptr<ResourceMetadataStorage> storage_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<ResourceMetadata> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMetadata);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_
