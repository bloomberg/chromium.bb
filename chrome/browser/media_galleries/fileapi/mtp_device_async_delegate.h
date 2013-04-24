// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "webkit/fileapi/async_file_util.h"

namespace base {
class FilePath;
}

namespace chrome {

// Asynchronous delegate for media transfer protocol (MTP) device to perform
// media device isolated file system operations. Class that implements this
// delegate does the actual communication with the MTP device.
// ScopedMTPDeviceMapEntry class manages the lifetime of the delegate via
// MTPDeviceMapService class. Member functions and callbacks runs on the IO
// thread.
class MTPDeviceAsyncDelegate {
 public:
  // A callback to be called when GetFileInfo method call succeeds.
  typedef base::Callback<
      void(const base::PlatformFileInfo& file_info)> GetFileInfoSuccessCallback;

  // A callback to be called when ReadDirectory method call succeeds.
  typedef base::Callback<
      void(const fileapi::AsyncFileUtil::EntryList& file_list,
           bool has_more)> ReadDirectorySuccessCallback;

  // A callback to be called when GetFileInfo/ReadDirectory/CreateSnapshot
  // method call fails.
  typedef base::Callback<
      void(base::PlatformFileError error)> ErrorCallback;

  // A callback to be called when CreateSnapshotFile method call succeeds.
  typedef base::Callback<
      void(const base::PlatformFileInfo& file_info,
           const base::FilePath& local_path)> CreateSnapshotFileSuccessCallback;

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

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
