// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_

#include <queue>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "webkit/fileapi/async_file_util.h"

namespace base {
class FilePath;
}

namespace chrome {

struct SnapshotRequestInfo;

// MTPDeviceDelegateImplLinux communicates with the media transfer protocol
// (MTP) device to complete file system operations. These operations are
// performed asynchronously. Instantiate this class per MTP device storage.
// MTPDeviceDelegateImplLinux lives on the IO thread.
// MTPDeviceDelegateImplLinux does a call-and-reply to the UI thread
// to dispatch the requests to MediaTransferProtocolManager.
class MTPDeviceDelegateImplLinux : public MTPDeviceAsyncDelegate {
 private:
  friend void CreateMTPDeviceAsyncDelegate(
      const std::string&,
      const CreateMTPDeviceAsyncDelegateCallback&);

  enum InitializationState {
    UNINITIALIZED = 0,
    PENDING_INIT,
    INITIALIZED
  };

  // Used to represent pending task details.
  struct PendingTaskInfo {
    PendingTaskInfo(const tracked_objects::Location& location,
                    const base::Closure& task);
    ~PendingTaskInfo();

    const tracked_objects::Location location;
    const base::Closure task;
  };

  // Should only be called by CreateMTPDeviceAsyncDelegate() factory call.
  // Defer the device initializations until the first file operation request.
  // Do all the initializations in EnsureInitAndRunTask() function.
  explicit MTPDeviceDelegateImplLinux(const std::string& device_location);

  // Destructed via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceDelegateImplLinux();

  // MTPDeviceAsyncDelegate:
  virtual void GetFileInfo(const base::FilePath& file_path,
                           const GetFileInfoSuccessCallback& success_callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void ReadDirectory(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void CancelPendingTasksAndDeleteDelegate() OVERRIDE;

  // Ensures the device is initialized for communication by doing a
  // call-and-reply to the UI thread. |task_info.task| runs on the UI thread.
  //
  // If the device is already initialized, post the |task_info.task| immediately
  // on the UI thread.
  //
  // If the device is uninitialized, store the |task_info| in a pending task
  // list and runs all the pending tasks once the device is successfully
  // initialized.
  void EnsureInitAndRunTask(const PendingTaskInfo& task_info);

  // Writes data from the device to the snapshot file path based on the
  // parameters in |current_snapshot_request_info_| by doing a call-and-reply to
  // the UI thread.
  //
  // |snapshot_file_info| specifies the metadata details of the snapshot file.
  void WriteDataIntoSnapshotFile(
      const base::PlatformFileInfo& snapshot_file_info);

  // Processes the next pending request.
  void ProcessNextPendingRequest();

  // Handles the device initialization event. |succeeded| indicates whether
  // device initialization succeeded.
  //
  // If the device is successfully initialized, runs the next pending task.
  void OnInitCompleted(bool succeeded);

  // Called when GetFileInfo() succeeds. |file_info| specifies the
  // requested file details. |success_callback| is invoked to notify the caller
  // about the requested file details.
  void OnDidGetFileInfo(const GetFileInfoSuccessCallback& success_callback,
                        const base::PlatformFileInfo& file_info);

  // Called when GetFileInfo() succeeds. GetFileInfo() is invoked to
  // get the |root| directory metadata details. |file_info| specifies the |root|
  // directory details.
  //
  // If |root| is a directory, post a task on the UI thread to read the |root|
  // directory file entries.
  //
  // If |root| is not a directory, |error_callback| is invoked to notify the
  // caller about the platform file error and process the next pending request.
  void OnDidGetFileInfoToReadDirectory(
      const std::string& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback,
      const base::PlatformFileInfo& file_info);

  // Called when GetFileInfo() succeeds. GetFileInfo() is invoked to
  // create the snapshot file of |snapshot_request_info.device_file_path|.
  // |file_info| specifies the device file metadata details.
  //
  // Posts a task on the UI thread to copy the data contents of the device file
  // to the snapshot file.
  void OnDidGetFileInfoToCreateSnapshotFile(
      scoped_ptr<SnapshotRequestInfo> snapshot_request_info,
      const base::PlatformFileInfo& file_info);

  // Called when ReadDirectory() succeeds.
  //
  // |file_list| contains the directory file entries.
  // |success_callback| is invoked to notify the caller about the directory
  // file entries.
  void OnDidReadDirectory(const ReadDirectorySuccessCallback& success_callback,
                          const fileapi::AsyncFileUtil::EntryList& file_list);

  // Called when WriteDataIntoSnapshotFile() succeeds.
  //
  // |snapshot_file_info| specifies the snapshot file metadata details.
  //
  // |current_snapshot_request_info_.success_callback| is invoked to notify the
  // caller about |snapshot_file_info|.
  void OnDidWriteDataIntoSnapshotFile(
      const base::PlatformFileInfo& snapshot_file_info,
      const base::FilePath& snapshot_file_path);

  // Called when WriteDataIntoSnapshotFile() fails.
  //
  // |error| specifies the platform file error code.
  //
  // |current_snapshot_request_info_.error_callback| is invoked to notify the
  // caller about |error|.
  void OnWriteDataIntoSnapshotFileError(base::PlatformFileError error);

  // Handles the device file |error|. |error_callback| is invoked to notify the
  // caller about the file error.
  void HandleDeviceFileError(const ErrorCallback& error_callback,
                             base::PlatformFileError error);

  // MTP device initialization state.
  InitializationState init_state_;

  // Used to make sure only one task is in progress at any time.
  bool task_in_progress_;

  // Registered file system device path. This path does not
  // correspond to a real device path (e.g. "/usb:2,2:81282").
  const base::FilePath device_path_;

  // MTP device storage name (e.g. "usb:2,2:81282").
  std::string storage_name_;

  // A list of pending tasks that needs to be run when the device is
  // initialized or when the current task in progress is complete.
  std::queue<PendingTaskInfo> pending_tasks_;

  // Used to track the current snapshot file request. A snapshot file is created
  // incrementally. CreateSnapshotFile request reads the device file and writes
  // to the snapshot file in chunks. In order to retain the order of the
  // snapshot file requests, make sure there is only one active snapshot file
  // request at any time.
  scoped_ptr<SnapshotRequestInfo> current_snapshot_request_info_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPDeviceDelegateImplLinux> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
