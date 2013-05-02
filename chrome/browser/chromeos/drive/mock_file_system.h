// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FILE_SYSTEM_H_

#include <string>

#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drive {

class FileSystemObserver;

// Mock for FileSystemInterface.
class MockFileSystem : public FileSystemInterface {
 public:
  MockFileSystem();
  virtual ~MockFileSystem();

  // FileSystemInterface overrides.
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD1(AddObserver, void(FileSystemObserver* observer));
  MOCK_METHOD1(RemoveObserver,
               void(FileSystemObserver* observer));
  MOCK_METHOD0(CheckForUpdates, void());
  MOCK_METHOD2(GetEntryInfoByResourceId,
               void(const std::string& resource_id,
                    const GetEntryInfoWithFilePathCallback& callback));
  MOCK_METHOD3(Search, void(const std::string& search_query,
                            const GURL& next_feed,
                            const SearchCallback& callback));
  MOCK_METHOD4(SearchMetadata, void(const std::string& query,
                                    int options,
                                    int at_most_num_matches,
                                    const SearchMetadataCallback& callback));
  MOCK_METHOD3(TransferFileFromRemoteToLocal,
               void(const base::FilePath& local_src_file_path,
                    const base::FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(TransferFileFromLocalToRemote,
               void(const base::FilePath& local_src_file_path,
                    const base::FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD2(OpenFile, void(const base::FilePath& file_path,
                              const OpenFileCallback& callback));
  MOCK_METHOD2(CloseFile, void(const base::FilePath& file_path,
                              const FileOperationCallback& callback));
  MOCK_METHOD3(Copy, void(const base::FilePath& src_file_path,
                          const base::FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Move, void(const base::FilePath& src_file_path,
                          const base::FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Remove, void(const base::FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback));
  MOCK_METHOD4(CreateDirectory,
               void(const base::FilePath& directory_path,
                    bool is_exclusive,
                    bool is_recursive,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(CreateFile,
               void(const base::FilePath& file_path,
                    bool is_exclusive,
                    const FileOperationCallback& callback));
  MOCK_METHOD2(Pin, void(const base::FilePath& file_path,
                         const FileOperationCallback& callback));
  MOCK_METHOD2(Unpin, void(const base::FilePath& file_path,
                           const FileOperationCallback& callback));
  MOCK_METHOD2(GetFileByPath,
               void(const base::FilePath& file_path,
                    const GetFileCallback& callback));
  MOCK_METHOD4(
      GetFileByResourceId,
      void(const std::string& resource_id,
           const DriveClientContext& context,
           const GetFileCallback& get_file_callback,
           const google_apis::GetContentCallback& get_content_callback));
  MOCK_METHOD4(
      GetFileContentByPath,
      void(const base::FilePath& file_path,
           const GetFileContentInitializedCallback& initialized_callback,
           const google_apis::GetContentCallback& get_content_callback,
           const FileOperationCallback& completion_callback));
  MOCK_METHOD3(UpdateFileByResourceId,
               void(const std::string& resource_id,
                    const DriveClientContext& context,
                    const FileOperationCallback& callback));
  MOCK_METHOD2(GetEntryInfoByPath, void(const base::FilePath& file_path,
                                        const GetEntryInfoCallback& callback));
  MOCK_METHOD2(ReadDirectoryByPath,
               void(const base::FilePath& file_path,
                    const ReadDirectoryWithSettingCallback& callback));
  MOCK_METHOD2(RefreshDirectory,
               void(const base::FilePath& file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD1(GetAvailableSpace,
               void(const GetAvailableSpaceCallback& callback));
  // This function is not mockable by gmock because scoped_ptr is not supported.
  virtual void AddUploadedFile(scoped_ptr<google_apis::ResourceEntry> entry,
                               const base::FilePath& file_content_path,
                               const FileOperationCallback& callback) OVERRIDE {
  }
  MOCK_METHOD1(GetMetadata,
               void(const GetFilesystemMetadataCallback& callback));
  MOCK_METHOD2(MarkCacheFileAsMounted,
               void(const base::FilePath& drive_file_path,
                    const OpenFileCallback& callback));
  MOCK_METHOD2(MarkCacheFileAsUnmounted,
               void(const base::FilePath& cache_file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(GetCacheEntryByResourceId,
               void(const std::string& resource_id,
                    const std::string& md5,
                    const GetCacheEntryCallback& callback));
  MOCK_METHOD2(IterateCache,
               void(const CacheIterateCallback& iteration_callback,
                    const base::Closure& completion_callback));
  MOCK_METHOD0(Reload, void());
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FILE_SYSTEM_H_
