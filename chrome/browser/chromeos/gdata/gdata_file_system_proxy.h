// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
#pragma once

#include "webkit/chromeos/fileapi/remote_file_system_proxy.h"

class Profile;

namespace fileapi {
class FileSystemURL;
}

namespace gdata {

class GDataEntry;
class GDataEntryProto;
class GDataDirectoryProto;
class GDataFileSystemInterface;

// The interface class for remote file system proxy.
class GDataFileSystemProxy : public fileapi::RemoteFileSystemProxyInterface {
 public:
  // |profile| is used to create GDataFileSystem, which is a per-profile
  // instance.
  explicit GDataFileSystemProxy(GDataFileSystemInterface* file_system);

  // fileapi::RemoteFileSystemProxyInterface overrides.
  virtual void GetFileInfo(
      const fileapi::FileSystemURL& url,
      const fileapi::FileSystemOperationInterface::GetMetadataCallback&
          callback) OVERRIDE;
  virtual void Copy(
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void Move(
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void ReadDirectory(const fileapi::FileSystemURL& url,
     const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
         callback) OVERRIDE;
  virtual void Remove(
      const fileapi::FileSystemURL& url, bool recursive,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateDirectory(
      const fileapi::FileSystemURL& file_url,
      bool exclusive,
      bool recursive,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void Truncate(
      const fileapi::FileSystemURL& file_url, int64 length,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateSnapshotFile(
      const fileapi::FileSystemURL& url,
      const fileapi::FileSystemOperationInterface::SnapshotFileCallback&
      callback) OVERRIDE;
  virtual void CreateWritableSnapshotFile(
      const fileapi::FileSystemURL& url,
      const fileapi::WritableSnapshotFile& callback) OVERRIDE;
  virtual void OpenFile(
      const fileapi::FileSystemURL& url,
      int file_flags,
      base::ProcessHandle peer_handle,
      const fileapi::FileSystemOperationInterface::OpenFileCallback&
          callback) OVERRIDE;
  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.

 protected:
  virtual ~GDataFileSystemProxy();

 private:
  // Checks if a given |url| belongs to this file system. If it does,
  // the call will return true and fill in |file_path| with a file path of
  // a corresponding element within this file system.
  static bool ValidateUrl(const fileapi::FileSystemURL& url,
                          FilePath* file_path);

  // Helper callback for relaying reply for metadata retrieval request to the
  // calling thread.
  void OnGetMetadata(
      const FilePath& file_path,
      const fileapi::FileSystemOperationInterface::GetMetadataCallback&
          callback,
      base::PlatformFileError error,
      const FilePath& entry_path,
      scoped_ptr<gdata::GDataEntryProto> entry_proto);

  // Helper callback for relaying reply for GetEntryInfoByPath() to the
  // calling thread.
  void OnGetEntryInfoByPath(
      const fileapi::FileSystemOperationInterface::SnapshotFileCallback&
          callback,
      base::PlatformFileError error,
      const FilePath& entry_path,
      scoped_ptr<GDataEntryProto> entry_proto);

  // Helper callback for relaying reply for ReadDirectory() to the calling
  // thread.
  void OnReadDirectory(
      const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
          callback,
      base::PlatformFileError error,
      bool hide_hosted_documents,
      scoped_ptr<GDataDirectoryProto> directory_proto);

  // Helper callback for relaying reply for CreateWritableSnapshotFile() to
  // the calling thread.
  void OnCreateWritableSnapshotFile(
      const FilePath& virtual_path,
      const fileapi::WritableSnapshotFile& callback,
      base::PlatformFileError result,
      const FilePath& local_path);

  // Helper callback for closing the local cache file and committing the dirty
  // flag. This is triggered when the callback for CreateWritableSnapshotFile
  // released the refcounted reference to the file.
  void CloseWritableSnapshotFile(
      const FilePath& virtual_path,
      const FilePath& local_path);

  // Invoked during Truncate() operation. This is called when a local modifiable
  // cache is ready for truncation.
  void OnFileOpenedForTruncate(
      const FilePath& virtual_path,
      int64 length,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback,
      base::PlatformFileError open_result,
      const FilePath& local_cache_path);

  // Invoked during Truncate() operation. This is called when the truncation of
  // a local cache file is finished on FILE thread.
  void DidTruncate(
      const FilePath& virtual_path,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback,
      base::PlatformFileError* truncate_result);

  // GDataFileSystemProxy is owned by Profile, which outlives
  // GDataFileSystemProxy, which is owned by CrosMountPointProvider (i.e. by
  // the time Profile is removed, the file manager is already gone). Hence
  // it's safe to use this as a raw pointer.
  GDataFileSystemInterface* file_system_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
