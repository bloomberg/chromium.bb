 // Copyright 2013 The Chromium Authors. All rights reserved.
 // Use of this source code is governed by a BSD-style license that can be
 // found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "webkit/fileapi/async_file_util.h"
#include "webkit/fileapi/media/mtp_device_async_delegate.h"

namespace chrome {

class MTPReadFileWorker;
struct SnapshotRequestInfo;

// MTPDeviceTaskHelper dispatches the media transfer protocol (MTP) device
// operation requests (such as GetFileInfo, ReadDirectory, CreateSnapshotFile,
// OpenStorage and CloseStorage) to the MediaTransferProtocolManager.
// MTPDeviceTaskHelper lives on the UI thread. MTPDeviceTaskHelperMapService
// owns the MTPDeviceTaskHelper objects. MTPDeviceTaskHelper is instantiated per
// MTP device storage.
class MTPDeviceTaskHelper {
 public:
  typedef base::Callback<void(bool succeeded)> OpenStorageCallback;

  typedef fileapi::MTPDeviceAsyncDelegate::GetFileInfoSuccessCallback
      GetFileInfoSuccessCallback;

  typedef base::Callback<void(const fileapi::AsyncFileUtil::EntryList&)>
      ReadDirectorySuccessCallback;

  typedef fileapi::MTPDeviceAsyncDelegate::ErrorCallback ErrorCallback;

  MTPDeviceTaskHelper();
  ~MTPDeviceTaskHelper();

  // Dispatches the request to the MediaTransferProtocolManager to open the MTP
  // storage for communication.
  //
  // |storage_name| specifies the name of the storage device.
  // |callback| is called when the OpenStorage request completes. |callback|
  // runs on the IO thread.
  void OpenStorage(const std::string& storage_name,
                   const OpenStorageCallback& callback);

  // Dispatches the GetFileInfoByPath request to the
  // MediaTransferProtocolManager.
  //
  // |file_path| specifies the relative of the file whose details are requested.
  //
  // If the file details are fetched successfully, |success_callback| is invoked
  // on the IO thread to notify the caller about the file details.
  //
  // If there is an error, |error_callback| is invoked on the IO thread to
  // notify the caller about the file error.
  void GetFileInfoByPath(
      const std::string& file_path,
      const GetFileInfoSuccessCallback& success_callback,
      const ErrorCallback& error_callback);

  // Dispatches the read directory request to the MediaTransferProtocolManager.
  //
  // |dir_path| specifies the directory file path.
  //
  // If the directory file entries are enumerated successfully,
  // |success_callback| is invoked on the IO thread to notify the caller about
  // the directory file entries.
  //
  // If there is an error, |error_callback| is invoked on the IO thread to
  // notify the caller about the file error.
  void ReadDirectoryByPath(const std::string& dir_path,
                           const ReadDirectorySuccessCallback& success_callback,
                           const ErrorCallback& error_callback);

  // Forwards the WriteDataIntoSnapshotFile request to the MTPReadFileWorker
  // object.
  //
  // |request_info| specifies the snapshot file request params.
  // |snapshot_file_info| specifies the metadata of the snapshot file.
  void WriteDataIntoSnapshotFile(
      const SnapshotRequestInfo& request_info,
      const base::PlatformFileInfo& snapshot_file_info);

  // Dispatches the CloseStorage request to the MediaTransferProtocolManager.
  void CloseStorage() const;

 private:
  // Query callback for OpenStorage() to run |callback| on the IO thread.
  //
  // If OpenStorage request succeeds, |error| is set to false and
  // |device_handle| contains the handle to communicate with the MTP device.
  //
  // If OpenStorage request fails, |error| is set to true and |device_handle| is
  // set to an empty string.
  void OnDidOpenStorage(const OpenStorageCallback& callback,
                        const std::string& device_handle,
                        bool error);

  // Query callback for GetFileInfo().
  //
  // If there is no error, |file_entry| will contain the
  // requested media device file details and |error| is set to false.
  // |success_callback| is invoked on the IO thread to notify the caller.
  //
  // If there is an error, |file_entry| is invalid and |error| is
  // set to true. |error_callback| is invoked on the IO thread to notify the
  // caller.
  void OnGetFileInfo(const GetFileInfoSuccessCallback& success_callback,
                     const ErrorCallback& error_callback,
                     const MtpFileEntry& file_entry,
                     bool error) const;

  // Query callback for ReadDirectoryByPath().
  //
  // If there is no error, |error| is set to false, |file_entries| has the
  // directory file entries and |success_callback| is invoked on the IO thread
  // to notify the caller.
  //
  // If there is an error, |error| is set to true, |file_entries| is empty
  // and |error_callback| is invoked on the IO thread to notify the caller.
  void OnDidReadDirectoryByPath(
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback,
      const std::vector<MtpFileEntry>& file_entries,
      bool error) const;

  // Called when the device is uninitialized.
  //
  // Runs |error_callback| on the IO thread to notify the caller about the
  // device |error|.
  void HandleDeviceError(const ErrorCallback& error_callback,
                         base::PlatformFileError error) const;

  // Handle to communicate with the MTP device.
  std::string device_handle_;

  // Used to handle WriteDataInfoSnapshotFile request.
  scoped_ptr<MTPReadFileWorker> read_file_worker_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPDeviceTaskHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceTaskHelper);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_H_
