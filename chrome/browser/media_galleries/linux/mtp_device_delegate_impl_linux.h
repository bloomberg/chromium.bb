// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/async_file_util.h"

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
    PendingTaskInfo(const base::FilePath& path,
                    content::BrowserThread::ID thread_id,
                    const tracked_objects::Location& location,
                    const base::Closure& task);
    ~PendingTaskInfo();

    base::FilePath path;
    base::FilePath cached_path;
    const content::BrowserThread::ID thread_id;
    const tracked_objects::Location location;
    const base::Closure task;
  };

  class MTPFileNode;

  // Maps file ids to file nodes.
  typedef std::map<uint32, MTPFileNode*> FileIdToMTPFileNodeMap;

  // Maps file paths to file info.
  typedef std::map<base::FilePath, fileapi::DirectoryEntry> FileInfoCache;

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
  virtual bool IsStreaming() OVERRIDE;
  virtual void ReadBytes(
      const base::FilePath& device_file_path,
      net::IOBuffer* buf, int64 offset, int buf_len,
      const ReadBytesSuccessCallback& success_callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void CancelPendingTasksAndDeleteDelegate() OVERRIDE;

  // The internal methods correspond to the similarly named methods above.
  // The |root_node_| cache should be filled at this point.
  virtual void GetFileInfoInternal(
      const base::FilePath& file_path,
      const GetFileInfoSuccessCallback& success_callback,
      const ErrorCallback& error_callback);
  virtual void ReadDirectoryInternal(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback);
  virtual void CreateSnapshotFileInternal(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback);
  virtual void ReadBytesInternal(
      const base::FilePath& device_file_path,
      net::IOBuffer* buf, int64 offset, int buf_len,
      const ReadBytesSuccessCallback& success_callback,
      const ErrorCallback& error_callback);

  // Ensures the device is initialized for communication.
  // If the device is already initialized, call RunTask().
  //
  // If the device is uninitialized, store the |task_info| in a pending task
  // queue and runs the pending tasks in the queue once the device is
  // successfully initialized.
  void EnsureInitAndRunTask(const PendingTaskInfo& task_info);

  // Runs a task. If |task_info.path| is empty, or if the path is cached, runs
  // the task immediately.
  // Otherwise, fills the cache first before running the task.
  // |task_info.task| runs on the UI thread.
  void RunTask(const PendingTaskInfo& task_info);

  // Writes data from the device to the snapshot file path based on the
  // parameters in |current_snapshot_request_info_| by doing a call-and-reply to
  // the UI thread.
  //
  // |snapshot_file_info| specifies the metadata details of the snapshot file.
  void WriteDataIntoSnapshotFile(const base::File::Info& snapshot_file_info);

  // Marks the current request as complete and call ProcessNextPendingRequest().
  void PendingRequestDone();

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
                        const base::File::Info& file_info);

  // Called when GetFileInfo() succeeds. GetFileInfo() is invoked to
  // get the |dir_id| directory metadata details. |file_info| specifies the
  // |dir_id| directory details.
  //
  // If |dir_id| is a directory, post a task on the UI thread to read the
  // |dir_id| directory file entries.
  //
  // If |dir_id| is not a directory, |error_callback| is invoked to notify the
  // caller about the file error and process the next pending request.
  void OnDidGetFileInfoToReadDirectory(
      uint32 dir_id,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback,
      const base::File::Info& file_info);

  // Called when GetFileInfo() succeeds. GetFileInfo() is invoked to
  // create the snapshot file of |snapshot_request_info.device_file_path|.
  // |file_info| specifies the device file metadata details.
  //
  // Posts a task on the UI thread to copy the data contents of the device file
  // to the snapshot file.
  void OnDidGetFileInfoToCreateSnapshotFile(
      scoped_ptr<SnapshotRequestInfo> snapshot_request_info,
      const base::File::Info& file_info);

  // Called when ReadDirectory() succeeds.
  //
  // |dir_id| is the directory read.
  // |success_callback| is invoked to notify the caller about the directory
  // file entries.
  // |file_list| contains the directory file entries with their file ids.
  // |has_more| is true if there are more file entries to read.
  void OnDidReadDirectory(uint32 dir_id,
                          const ReadDirectorySuccessCallback& success_callback,
                          const fileapi::AsyncFileUtil::EntryList& file_list,
                          bool has_more);

  // Called when WriteDataIntoSnapshotFile() succeeds.
  //
  // |snapshot_file_info| specifies the snapshot file metadata details.
  //
  // |current_snapshot_request_info_.success_callback| is invoked to notify the
  // caller about |snapshot_file_info|.
  void OnDidWriteDataIntoSnapshotFile(
      const base::File::Info& snapshot_file_info,
      const base::FilePath& snapshot_file_path);

  // Called when WriteDataIntoSnapshotFile() fails.
  //
  // |error| specifies the file error code.
  //
  // |current_snapshot_request_info_.error_callback| is invoked to notify the
  // caller about |error|.
  void OnWriteDataIntoSnapshotFileError(base::File::Error error);

  // Called when ReadBytes() succeeds.
  //
  // |success_callback| is invoked to notify the caller about the read bytes.
  // |bytes_read| is the number of bytes read.
  void OnDidReadBytes(const ReadBytesSuccessCallback& success_callback,
                      const base::File::Info& file_info, int bytes_read);

  // Called when FillFileCache() succeeds.
  void OnDidFillFileCache(const base::FilePath& path,
                          const fileapi::AsyncFileUtil::EntryList& file_list,
                          bool has_more);

  // Called when FillFileCache() fails.
  void OnFillFileCacheFailed(base::File::Error error);

  // Handles the device file |error| while operating on |file_id|.
  // |error_callback| is invoked to notify the caller about the file error.
  void HandleDeviceFileError(const ErrorCallback& error_callback,
                             uint32 file_id,
                             base::File::Error error);

  // Given a full path, returns a non-empty sub-path that needs to be read into
  // the cache if such a uncached path exists.
  // |cached_path| is the portion of |path| that has had cache lookup attempts.
  base::FilePath NextUncachedPathComponent(
      const base::FilePath& path,
      const base::FilePath& cached_path) const;

  // Fills the file cache using the results from NextUncachedPathComponent().
  void FillFileCache(const base::FilePath& uncached_path);

  // Given a full path, if it exists in the cache, writes the file's id to |id|
  // and return true.
  bool CachedPathToId(const base::FilePath& path, uint32* id) const;

  // MTP device initialization state.
  InitializationState init_state_;

  // Used to make sure only one task is in progress at any time.
  // Otherwise the browser will try to send too many requests at once and
  // overload the device.
  bool task_in_progress_;

  // Registered file system device path. This path does not
  // correspond to a real device path (e.g. "/usb:2,2:81282").
  const base::FilePath device_path_;

  // MTP device storage name (e.g. "usb:2,2:81282").
  std::string storage_name_;

  // A list of pending tasks that needs to be run when the device is
  // initialized or when the current task in progress is complete.
  std::deque<PendingTaskInfo> pending_tasks_;

  // Used to track the current snapshot file request. A snapshot file is created
  // incrementally. CreateSnapshotFile request reads the device file and writes
  // to the snapshot file in chunks. In order to retain the order of the
  // snapshot file requests, make sure there is only one active snapshot file
  // request at any time.
  scoped_ptr<SnapshotRequestInfo> current_snapshot_request_info_;

  // A mapping for quick lookups into the |root_node_| tree structure. Since
  // |root_node_| contains pointers to this map, it must be declared after this
  // so destruction happens in the right order.
  FileIdToMTPFileNodeMap file_id_to_node_map_;

  // The root node of a tree-structure that caches the directory structure of
  // the MTP device.
  scoped_ptr<MTPFileNode> root_node_;

  // A list of child nodes encountered while a ReadDirectory operation, which
  // can return results over multiple callbacks, is in progress.
  std::set<std::string> child_nodes_seen_;

  // A cache to store file metadata for file entries read during a ReadDirectory
  // operation. Used to service incoming GetFileInfo calls for the duration of
  // the ReadDirectory operation.
  FileInfoCache file_info_cache_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPDeviceDelegateImplLinux> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplLinux);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
