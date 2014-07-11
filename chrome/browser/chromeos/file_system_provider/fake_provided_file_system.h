// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"

class Profile;

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {

class RequestManager;

extern const char kFakeFileName[];
extern const char kFakeFilePath[];
extern const char kFakeFileText[];
extern const size_t kFakeFileSize;
extern const char kFakeFileModificationTime[];

// Fake provided file system implementation. Does not communicate with target
// extensions. Used for unit tests.
class FakeProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  explicit FakeProvidedFileSystem(
      const ProvidedFileSystemInfo& file_system_info);
  virtual ~FakeProvidedFileSystem();

  // ProvidedFileSystemInterface overrides.
  virtual void RequestUnmount(
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const base::FilePath& entry_path,
      const ProvidedFileSystemInterface::GetMetadataCallback& callback)
      OVERRIDE;
  virtual void ReadDirectory(
      const base::FilePath& directory_path,
      const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenFileMode mode,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void CloseFile(
      int file_handle,
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual void ReadFile(int file_handle,
                        net::IOBuffer* buffer,
                        int64 offset,
                        int length,
                        const ReadChunkReceivedCallback& callback) OVERRIDE;
  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      bool exclusive,
      bool recursive,
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const OVERRIDE;
  virtual RequestManager* GetRequestManager() OVERRIDE;
  virtual base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() OVERRIDE;

  // Factory callback, to be used in Service::SetFileSystemFactory(). The
  // |event_router| argument can be NULL.
  static ProvidedFileSystemInterface* Create(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info);

 private:
  typedef std::map<int, base::FilePath> OpenedFilesMap;

  ProvidedFileSystemInfo file_system_info_;
  OpenedFilesMap opened_files_;
  int last_file_handle_;

  base::WeakPtrFactory<ProvidedFileSystemInterface> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FakeProvidedFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_
