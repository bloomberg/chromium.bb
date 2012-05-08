// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gdata {

// Mock for GDataFileSystemInterface.
class MockGDataFileSystem : public GDataFileSystemInterface {
 public:
  MockGDataFileSystem();
  virtual ~MockGDataFileSystem();

  // GDataFileSystemInterface overrides.
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD0(StartUpdates, void());
  MOCK_METHOD0(StopUpdates, void());
  MOCK_METHOD0(CheckForUpdates, void());
  MOCK_METHOD1(Authenticate, void(const AuthStatusCallback& callback));
  MOCK_METHOD2(FindEntryByResourceIdSync, void(const std::string& resource_id,
                                               FindEntryDelegate* delegate));
  MOCK_METHOD2(SearchAsync, void(const std::string& search_query,
                                const ReadDirectoryCallback& callback));
  MOCK_METHOD3(TransferFileFromRemoteToLocal,
               void(const FilePath& local_src_file_path,
                    const FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(TransferFileFromLocalToRemote,
               void(const FilePath& local_src_file_path,
                    const FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(Copy, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Move, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Remove, void(const FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback));
  MOCK_METHOD4(CreateDirectory,
               void(const FilePath& directory_path,
                    bool is_exclusive,
                    bool is_recursive,
                    const FileOperationCallback& callback));
  MOCK_METHOD3(GetFileByPath,
               void(const FilePath& file_path,
                    const GetFileCallback& get_file_callback,
                    const GetDownloadDataCallback& get_download_data_callback));
  MOCK_METHOD3(GetFileByResourceId,
               void(const std::string& resource_id,
                    const GetFileCallback& get_file_callback,
                    const GetDownloadDataCallback& get_download_data_callback));
  MOCK_METHOD0(GetOperationRegistry, GDataOperationRegistry*());
  MOCK_METHOD3(GetCacheState, void(const std::string& resource_id,
                                   const std::string& md5,
                                   const GetCacheStateCallback& callback));
  MOCK_METHOD2(GetFileInfoByPathAsync,
               void(const FilePath& file_path,
                    const GetFileInfoCallback& callback));
  MOCK_METHOD2(GetEntryInfoByPathAsync,
               void(const FilePath& file_path,
                    const GetEntryInfoCallback& callback));
  MOCK_METHOD2(ReadDirectoryByPathAsync,
               void(const FilePath& file_path,
                    const ReadDirectoryCallback& callback));
  MOCK_METHOD2(GetFileInfoByPath, bool(const FilePath& file_path,
                                       GDataFileProperties* properties));
  MOCK_CONST_METHOD1(IsUnderGDataCacheDirectory, bool(const FilePath& path));
  MOCK_CONST_METHOD1(GetCacheDirectoryPath, FilePath(
      GDataRootDirectory::CacheSubDirectoryType));
  MOCK_CONST_METHOD4(GetCacheFilePath, FilePath(
      const std::string&,
      const std::string&,
      GDataRootDirectory::CacheSubDirectoryType,
      CachedFileOrigin));
  MOCK_METHOD1(GetAvailableSpace,
               void(const GetAvailableSpaceCallback& callback));
  MOCK_METHOD3(SetPinState, void(const FilePath&,
                                 bool,
                                 const FileOperationCallback& callback));
  MOCK_METHOD3(SetMountedState, void(const FilePath&,
                                     bool,
                                     const SetMountedStateCallback& callback));
  MOCK_METHOD4(AddUploadedFile, void(const FilePath& file,
                                     DocumentEntry* entry,
                                     const FilePath& file_content_path,
                                     FileOperationType cache_operation));
  MOCK_METHOD0(hide_hosted_documents, bool());
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
