// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_MEDIA_TRANSFER_PROTOCOL_MANAGER_LINUX_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_MEDIA_TRANSFER_PROTOCOL_MANAGER_LINUX_H_

#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chrome {

// A dummy MediaTransferProtocolManager implementation.
class TestMediaTransferProtocolManagerLinux
    : public device::MediaTransferProtocolManager {
 public:
  TestMediaTransferProtocolManagerLinux();
  virtual ~TestMediaTransferProtocolManagerLinux();

 private:
  // device::MediaTransferProtocolManager implementation.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const std::vector<std::string> GetStorages() const OVERRIDE;
  virtual const MtpStorageInfo* GetStorageInfo(
      const std::string& storage_name) const OVERRIDE;
  virtual void OpenStorage(const std::string& storage_name,
                           const std::string& mode,
                           const OpenStorageCallback& callback) OVERRIDE;
  virtual void CloseStorage(const std::string& storage_handle,
                            const CloseStorageCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const std::string& storage_handle,
      const std::string& path,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void ReadDirectoryById(
      const std::string& storage_handle,
      uint32 file_id,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void ReadFileChunkByPath(const std::string& storage_handle,
                                   const std::string& path,
                                   uint32 offset,
                                   uint32 count,
                                   const ReadFileCallback& callback) OVERRIDE;
  virtual void ReadFileChunkById(const std::string& storage_handle,
                                 uint32 file_id,
                                 uint32 offset,
                                 uint32 count,
                                 const ReadFileCallback& callback) OVERRIDE;
  virtual void GetFileInfoByPath(const std::string& storage_handle,
                                 const std::string& path,
                                 const GetFileInfoCallback& callback) OVERRIDE;
  virtual void GetFileInfoById(const std::string& storage_handle,
                               uint32 file_id,
                               const GetFileInfoCallback& callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TestMediaTransferProtocolManagerLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_MEDIA_TRANSFER_PROTOCOL_MANAGER_LINUX_H_
