// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/async_file_util.h"

class EventRouter;

namespace base {
class Time;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInfo;
class RequestManager;

// Represents metadata for either a file or a directory. Returned by GetMetadata
// method in ProvidedFileSystemInterface.
struct EntryMetadata {
  EntryMetadata();
  ~EntryMetadata();

  bool is_directory;
  std::string name;
  int64 size;
  base::Time modification_time;
  std::string mime_type;
};

// Interface for a provided file system. Acts as a proxy between providers
// and clients.
// TODO(mtomasz): Add more methods once implemented.
class ProvidedFileSystemInterface {
 public:
  typedef base::Callback<void(int file_handle, base::File::Error result)>
      OpenFileCallback;

  typedef base::Callback<
      void(int chunk_length, bool has_more, base::File::Error result)>
      ReadChunkReceivedCallback;

  typedef base::Callback<void(const EntryMetadata& entry_metadata,
                              base::File::Error result)> GetMetadataCallback;

  // Mode of opening a file. Used by OpenFile().
  enum OpenFileMode { OPEN_FILE_MODE_READ, OPEN_FILE_MODE_WRITE };

  virtual ~ProvidedFileSystemInterface() {}

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code.
  virtual void RequestUnmount(
      const fileapi::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests metadata of the passed |entry_path|. It can be either a file
  // or a directory.
  virtual void GetMetadata(const base::FilePath& entry_path,
                           const GetMetadataCallback& callback) = 0;

  // Requests enumerating entries from the passed |directory_path|. The callback
  // can be called multiple times until |has_more| is set to false.
  virtual void ReadDirectory(
      const base::FilePath& directory_path,
      const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) = 0;

  // Requests opening a file at |file_path|. If the file doesn't exist, then the
  // operation will fail.
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenFileMode mode,
                        const OpenFileCallback& callback) = 0;

  // Requests closing a file, previously opened with OpenFile() as a file with
  // |file_handle|. For either succes or error |callback| must be called.
  virtual void CloseFile(
      int file_handle,
      const fileapi::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests reading a file previously opened with |file_handle|. The callback
  // can be called multiple times until |has_more| is set to false. On success
  // it should return |length| bytes starting from |offset| in total. It can
  // return less only in case EOF is encountered.
  virtual void ReadFile(int file_handle,
                        net::IOBuffer* buffer,
                        int64 offset,
                        int length,
                        const ReadChunkReceivedCallback& callback) = 0;

  // Requests creating a directory. If |recursive| is passed, then all non
  // existing directories on the path will be created. If |exclusive| is true,
  // then creating the directory will fail, if it already exists.
  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      bool exclusive,
      bool recursive,
      const fileapi::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests deleting a directory. If |recursive| is passed and the entry is
  // a directory, then all contents of it (recursively) will be deleted too.
  virtual void DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const fileapi::AsyncFileUtil::StatusCallback& callback) = 0;

  // Returns a provided file system info for this file system.
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const = 0;

  // Returns a request manager for the file system.
  virtual RequestManager* GetRequestManager() = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
