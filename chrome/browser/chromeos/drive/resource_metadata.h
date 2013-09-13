// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {

typedef std::vector<ResourceEntry> ResourceEntryVector;

// Used to get a resource entry from the file system.
// If |error| is not FILE_ERROR_OK, |entry_info| is set to NULL.
typedef base::Callback<void(FileError error,
                            scoped_ptr<ResourceEntry> entry)>
    GetResourceEntryCallback;

typedef base::Callback<void(FileError error,
                            scoped_ptr<ResourceEntryVector> entries)>
    ReadDirectoryCallback;

typedef base::Callback<void(int64)> GetChangestampCallback;

typedef base::Callback<void(const ResourceEntry& entry)> IterateCallback;

namespace internal {

// Storage for Drive Metadata.
// All methods must be run with |blocking_task_runner| unless otherwise noted.
class ResourceMetadata {
 public:
  typedef ResourceMetadataStorage::Iterator Iterator;

  ResourceMetadata(
      ResourceMetadataStorage* storage,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Initializes this object.
  // This method should be called before any other methods.
  FileError Initialize() WARN_UNUSED_RESULT;

  // Destroys this object.  This method posts a task to |blocking_task_runner_|
  // to safely delete this object.
  // Must be called on the UI thread.
  void Destroy();

  // Resets this object.
  FileError Reset();

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  // Must be called on the UI thread.
  void GetLargestChangestampOnUIThread(const GetChangestampCallback& callback);
  void SetLargestChangestampOnUIThread(int64 value,
                                       const FileOperationCallback& callback);

  // Synchronous version of GetLargestChangestampOnUIThread.
  int64 GetLargestChangestamp();

  // Synchronous version of SetLargestChangestampOnUIThread.
  FileError SetLargestChangestamp(int64 value);

  // Adds |entry| to the metadata tree based on its parent_local_id.
  FileError AddEntry(const ResourceEntry& entry, std::string* out_id);

  // Removes entry with |id| from its parent.
  FileError RemoveEntry(const std::string& id);

  // Finds an entry (a file or a directory) by |id|.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void GetResourceEntryByIdOnUIThread(const std::string& id,
                                      const GetResourceEntryCallback& callback);

  // Synchronous version of GetResourceEntryByIdOnUIThread().
  FileError GetResourceEntryById(const std::string& id,
                                 ResourceEntry* out_entry);

  // Finds an entry (a file or a directory) by |file_path|.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void GetResourceEntryByPathOnUIThread(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback);

  // Synchronous version of GetResourceEntryByPathOnUIThread().
  FileError GetResourceEntryByPath(const base::FilePath& file_path,
                                   ResourceEntry* out_entry);

  // Finds and reads a directory by |file_path|.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void ReadDirectoryByPathOnUIThread(const base::FilePath& file_path,
                                     const ReadDirectoryCallback& callback);

  // Synchronous version of ReadDirectoryByPathOnUIThread().
  FileError ReadDirectoryByPath(const base::FilePath& file_path,
                                ResourceEntryVector* out_entries);

  // Replaces an existing entry whose ID is |id| with |entry|.
  FileError RefreshEntry(const std::string& id, const ResourceEntry& entry);

  // Recursively gets directories under the entry pointed to by |id|.
  void GetSubDirectoriesRecursively(const std::string& id,
                                    std::set<base::FilePath>* sub_directories);

  // Returns the id of the resource named |base_name| directly under
  // the directory with |parent_local_id|.
  // If not found, empty string will be returned.
  std::string GetChildId(const std::string& parent_local_id,
                         const std::string& base_name);

  // Returns an object to iterate over entries.
  scoped_ptr<Iterator> GetIterator();

  // Returns virtual file path of the entry.
  base::FilePath GetFilePath(const std::string& id);

  // Returns ID of the entry at the given path.
  FileError GetIdByPath(const base::FilePath& file_path, std::string* out_id);

  // Returns the local ID associated with the given resource ID.
  FileError GetIdByResourceId(const std::string& resource_id,
                              std::string* out_local_id);

 private:
  // Note: Use Destroy() to delete this object.
  ~ResourceMetadata();

  // Sets up entries which should be present by default.
  bool SetUpDefaultEntries();

  // Used to implement Destroy().
  void DestroyOnBlockingPool();

  // Puts an entry under its parent directory. Removes the child from the old
  // parent if there is. This method will also do name de-duplication to ensure
  // that the exposed presentation path does not have naming conflicts. Two
  // files with the same name "Foo" will be renamed to "Foo (1)" and "Foo (2)".
  // |id| is used as the ID of the entry.
  bool PutEntryUnderDirectory(const std::string& id,
                              const ResourceEntry& entry);

  // Removes the entry and its descendants.
  bool RemoveEntryRecursively(const std::string& id);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  ResourceMetadataStorage* storage_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<ResourceMetadata> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMetadata);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_METADATA_H_
