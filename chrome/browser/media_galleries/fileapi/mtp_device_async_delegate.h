// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/fileapi/async_file_util.h"

namespace base {
class FilePath;
}

namespace net {
class IOBuffer;
}

// Asynchronous delegate for media transfer protocol (MTP) device to perform
// media device file system operations. Class that implements this
// delegate does the actual communication with the MTP device.
// The lifetime of the delegate is managed by the MTPDeviceMapService class.
// Member functions and callbacks run on the IO thread.
class MTPDeviceAsyncDelegate {
 public:
  // A callback to be called when GetFileInfo method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info)> GetFileInfoSuccessCallback;

  // A callback to be called when ReadDirectory method call succeeds.
  typedef base::Callback<
      void(const storage::AsyncFileUtil::EntryList& file_list, bool has_more)>
      ReadDirectorySuccessCallback;

  // A callback to be called when GetFileInfo/ReadDirectory/CreateSnapshot
  // method call fails.
  typedef base::Callback<void(base::File::Error error)> ErrorCallback;

  // A callback to be called when CreateSnapshotFile method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info,
           const base::FilePath& local_path)> CreateSnapshotFileSuccessCallback;

  // A callback to be called when ReadBytes method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info,
           int bytes_read)> ReadBytesSuccessCallback;

  struct ReadBytesRequest {
    ReadBytesRequest(uint32 file_id,
                     net::IOBuffer* buf, int64 offset, int buf_len,
                     const ReadBytesSuccessCallback& success_callback,
                     const ErrorCallback& error_callback);
    ~ReadBytesRequest();

    uint32 file_id;
    scoped_refptr<net::IOBuffer> buf;
    int64 offset;
    int buf_len;
    ReadBytesSuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  // Gets information about the given |file_path| and invokes the appropriate
  // callback asynchronously when complete.
  virtual void GetFileInfo(
      const base::FilePath& file_path,
      const GetFileInfoSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Enumerates the |root| directory contents and invokes the appropriate
  // callback asynchronously when complete.
  virtual void ReadDirectory(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Copy the contents of |device_file_path| to |local_path|. Invokes the
  // appropriate callback asynchronously when complete.
  virtual void CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Platform-specific implementations that are streaming don't create a local
  // snapshot file. Blobs are instead FileSystemURL backed and read in a stream.
  virtual bool IsStreaming() = 0;

  // Reads up to |buf_len| bytes from |device_file_path| into |buf|. Invokes the
  // appropriate callback asynchronously when complete. Only valid when
  // IsStreaming() is true.
  virtual void ReadBytes(const base::FilePath& device_file_path,
                         const scoped_refptr<net::IOBuffer>& buf,
                         int64 offset,
                         int buf_len,
                         const ReadBytesSuccessCallback& success_callback,
                         const ErrorCallback& error_callback) = 0;

  // Called when the
  // (1) Browser application is in shutdown mode (or)
  // (2) Last extension using this MTP device is destroyed (or)
  // (3) Attached MTP device is removed (or)
  // (4) User revoked the MTP device gallery permission.
  // Ownership of |MTPDeviceAsyncDelegate| is handed off to the delegate
  // implementation class by this call. This function should take care of
  // cancelling all the pending tasks before deleting itself.
  virtual void CancelPendingTasksAndDeleteDelegate() = 0;

 protected:
  // Always destruct this object via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceAsyncDelegate() {}
};

typedef base::Callback<void(MTPDeviceAsyncDelegate*)>
    CreateMTPDeviceAsyncDelegateCallback;

void CreateMTPDeviceAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& callback);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
