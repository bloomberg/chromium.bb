// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/fileapi/async_file_util.h"

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
  std::string thumbnail;

 private:
  DISALLOW_COPY_AND_ASSIGN(EntryMetadata);
};

// Interface for a provided file system. Acts as a proxy between providers
// and clients. All of the request methods return an abort callback in order to
// terminate it while running. They must be called on the same thread as the
// request methods. The cancellation callback may be null if the operation
// fails synchronously.
class ProvidedFileSystemInterface {
 public:
  // Mode of opening a file. Used by OpenFile().
  enum OpenFileMode { OPEN_FILE_MODE_READ, OPEN_FILE_MODE_WRITE };

  // Extra fields to be fetched with metadata.
  enum MetadataField {
    METADATA_FIELD_DEFAULT = 0,
    METADATA_FIELD_THUMBNAIL = 1 << 0
  };

  typedef base::Callback<void(int file_handle, base::File::Error result)>
      OpenFileCallback;

  typedef base::Callback<
      void(int chunk_length, bool has_more, base::File::Error result)>
      ReadChunkReceivedCallback;

  typedef base::Callback<void(scoped_ptr<EntryMetadata> entry_metadata,
                              base::File::Error result)> GetMetadataCallback;

  typedef base::Callback<void(
      const storage::AsyncFileUtil::StatusCallback& callback)> AbortCallback;

  // Mask of fields requested from the GetMetadata() call.
  typedef int MetadataFieldMask;

  virtual ~ProvidedFileSystemInterface() {}

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code.
  virtual AbortCallback RequestUnmount(
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests metadata of the passed |entry_path|. It can be either a file
  // or a directory. All |fields| will be returned if supported. Note, that
  // default fields are always returned.
  virtual AbortCallback GetMetadata(const base::FilePath& entry_path,
                                    MetadataFieldMask fields,
                                    const GetMetadataCallback& callback) = 0;

  // Requests enumerating entries from the passed |directory_path|. The callback
  // can be called multiple times until |has_more| is set to false.
  virtual AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      const storage::AsyncFileUtil::ReadDirectoryCallback& callback) = 0;

  // Requests opening a file at |file_path|. If the file doesn't exist, then the
  // operation will fail.
  virtual AbortCallback OpenFile(const base::FilePath& file_path,
                                 OpenFileMode mode,
                                 const OpenFileCallback& callback) = 0;

  // Requests closing a file, previously opened with OpenFile() as a file with
  // |file_handle|. For either succes or error |callback| must be called.
  virtual AbortCallback CloseFile(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests reading a file previously opened with |file_handle|. The callback
  // can be called multiple times until |has_more| is set to false. On success
  // it should return |length| bytes starting from |offset| in total. It can
  // return less only in case EOF is encountered.
  virtual AbortCallback ReadFile(int file_handle,
                                 net::IOBuffer* buffer,
                                 int64 offset,
                                 int length,
                                 const ReadChunkReceivedCallback& callback) = 0;

  // Requests creating a directory. If |recursive| is passed, then all non
  // existing directories on the path will be created. The operation will fail
  // if the target directory already exists.
  virtual AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests creating a file. If the entry already exists, then the
  // FILE_ERROR_EXISTS error must be returned.
  virtual AbortCallback CreateFile(
      const base::FilePath& file_path,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests deleting a directory. If |recursive| is passed and the entry is
  // a directory, then all contents of it (recursively) will be deleted too.
  virtual AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests copying an entry (recursively in case of a directory) within the
  // same file system.
  virtual AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests moving an entry (recursively in case of a directory) within the
  // same file system.
  virtual AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests truncating a file to the desired length.
  virtual AbortCallback Truncate(
      const base::FilePath& file_path,
      int64 length,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests writing to a file previously opened with |file_handle|.
  virtual AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64 offset,
      int length,
      const storage::AsyncFileUtil::StatusCallback& callback) = 0;

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
