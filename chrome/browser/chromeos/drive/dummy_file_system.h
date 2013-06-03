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
  virtual void Initialize() OVERRIDE {}
  virtual void AddObserver(FileSystemObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(FileSystemObserver* observer) OVERRIDE {}
  virtual void CheckForUpdates() OVERRIDE {}
  virtual void GetResourceEntryById(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE {}
  virtual void TransferFileFromRemoteToLocal(
      const base::FilePath& remote_src_file_path,
      const base::FilePath& local_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void OpenFile(const base::FilePath& file_path,
                        const OpenFileCallback& callback) OVERRIDE {}
  virtual void CloseFile(const base::FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE {}
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE {}
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE {}
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) OVERRIDE {}
  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE {}
  virtual void TouchFile(const base::FilePath& file_path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const FileOperationCallback& callback) OVERRIDE {}
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) OVERRIDE {}
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) OVERRIDE {}
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE {}
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE {}
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) OVERRIDE {}
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void GetResourceEntryByPath(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE {}
  virtual void ReadDirectoryByPath(
      const base::FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback) OVERRIDE {}
  virtual void RefreshDirectory(
      const base::FilePath& file_path,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void Search(const std::string& search_query,
                      const GURL& next_url,
                      const SearchCallback& callback) OVERRIDE {}
  virtual void SearchMetadata(
      const std::string& query,
      int options,
      int at_most_num_matches,
      const SearchMetadataCallback& callback) OVERRIDE {}
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE {}
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) OVERRIDE {}
  virtual void MarkCacheFileAsMounted(
      const base::FilePath& drive_file_path,
      const OpenFileCallback& callback) OVERRIDE {}
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) OVERRIDE {}
  virtual void GetCacheEntryByResourceId(
      const std::string& resource_id,
      const std::string& md5,
      const GetCacheEntryCallback& callback) OVERRIDE {}
  virtual void Reload() OVERRIDE {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DUMMY_FILE_SYSTEM_H_
