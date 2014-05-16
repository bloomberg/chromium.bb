// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

// Provided file system implementation. Forwards requests between providers and
// clients.
class ProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  ProvidedFileSystem(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info);
  virtual ~ProvidedFileSystem();

  // ProvidedFileSystemInterface overrides.
  virtual void RequestUnmount(
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const base::FilePath& entry_path,
      const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectory(
      const base::FilePath& directory_path,
      const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenFileMode mode,
                        bool create,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void CloseFile(
      int file_handle,
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual void ReadFile(int file_handle,
                        net::IOBuffer* buffer,
                        int64 offset,
                        int length,
                        const ReadChunkReceivedCallback& callback) OVERRIDE;
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const OVERRIDE;
  virtual RequestManager* GetRequestManager() OVERRIDE;

 private:
  extensions::EventRouter* event_router_;
  RequestManager request_manager_;
  ProvidedFileSystemInfo file_system_info_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
