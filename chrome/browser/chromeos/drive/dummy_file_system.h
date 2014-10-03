// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DUMMY_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DUMMY_FILE_SYSTEM_H_

#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace drive {

// Dummy implementation of FileSystemInterface. All functions do nothing.
class DummyFileSystem : public FileSystemInterface {
 public:
  virtual ~DummyFileSystem() {}
  virtual void AddObserver(FileSystemObserver* observer) override {}
  virtual void RemoveObserver(FileSystemObserver* observer) override {}
  virtual void CheckForUpdates() override {}
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) override {}
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenMode open_mode,
                        const std::string& mime_type,
                        const OpenFileCallback& callback) override {}
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    bool preserve_last_modified,
                    const FileOperationCallback& callback) override {}
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) override {}
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) override {}
  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback) override {}
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const std::string& mime_type,
                          const FileOperationCallback& callback) override {}
  virtual void TouchFile(const base::FilePath& file_path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const FileOperationCallback& callback) override {}
  virtual void TruncateFile(const base::FilePath& file_path,
                            int64 length,
                            const FileOperationCallback& callback) override {}
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) override {}
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) override {}
  virtual void GetFile(const base::FilePath& file_path,
                       const GetFileCallback& callback) override {}
  virtual void GetFileForSaving(const base::FilePath& file_path,
                                const GetFileCallback& callback) override {}
  virtual base::Closure GetFileContent(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) override;
  virtual void GetResourceEntry(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) override {}
  virtual void ReadDirectory(
      const base::FilePath& file_path,
      const ReadDirectoryEntriesCallback& entries_callback,
      const FileOperationCallback& completion_callback) override {}
  virtual void Search(const std::string& search_query,
                      const GURL& next_link,
                      const SearchCallback& callback) override {}
  virtual void SearchMetadata(
      const std::string& query,
      int options,
      int at_most_num_matches,
      const SearchMetadataCallback& callback) override {}
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) override {}
  virtual void GetShareUrl(const base::FilePath& file_path,
                           const GURL& embed_origin,
                           const GetShareUrlCallback& callback) override {}
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) override {}
  virtual void MarkCacheFileAsMounted(
      const base::FilePath& drive_file_path,
      const MarkMountedCallback& callback) override {}
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) override {}
  virtual void AddPermission(const base::FilePath& drive_file_path,
                             const std::string& email,
                             google_apis::drive::PermissionRole role,
                             const FileOperationCallback& callback) override {}
  virtual void Reset(const FileOperationCallback& callback) override {}
  virtual void GetPathFromResourceId(const std::string& resource_id,
                                     const GetFilePathCallback& callback)
      override {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DUMMY_FILE_SYSTEM_H_
