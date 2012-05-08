// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
#pragma once

#include "webkit/chromeos/fileapi/remote_file_system_proxy.h"

class Profile;

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
  virtual ~GDataFileSystemProxy();

  // fileapi::RemoteFileSystemProxyInterface overrides.
  virtual void GetFileInfo(const GURL& path,
      const fileapi::FileSystemOperationInterface::GetMetadataCallback&
          callback) OVERRIDE;
  virtual void Copy(const GURL& src_path, const GURL& dest_path,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void Move(const GURL& src_path, const GURL& dest_path,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void ReadDirectory(const GURL& path,
     const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
         callback) OVERRIDE;
  virtual void Remove(const GURL& path, bool recursive,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateDirectory(
      const GURL& file_url,
      bool exclusive,
      bool recursive,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateSnapshotFile(
      const GURL& path,
      const fileapi::FileSystemOperationInterface::SnapshotFileCallback&
      callback) OVERRIDE;
  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.

 private:
  // Checks if a given |url| belongs to this file system. If it does,
  // the call will return true and fill in |file_path| with a file path of
  // a corresponding element within this file system.
  static bool ValidateUrl(const GURL& url, FilePath* file_path);

  // Helper callback for relaying reply for metadata retrieval request to the
  // calling thread.
  void OnGetMetadata(
      const FilePath& file_path,
      const fileapi::FileSystemOperationInterface::GetMetadataCallback&
          callback,
      base::PlatformFileError error,
      const FilePath& entry_path,
      scoped_ptr<gdata::GDataEntryProto> entry_proto);

  // Helper callback for relaying reply for ReadDirectory() to the calling
  // thread.
  void OnReadDirectory(
      bool hide_hosted_documents,
      const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
          callback,
      base::PlatformFileError error,
      scoped_ptr<GDataDirectoryProto> directory_proto);

  // GDataFileSystemProxy is owned by Profile, which outlives
  // GDataFileSystemProxy, which is owned by CrosMountPointProvider (i.e. by
  // the time Profile is removed, the file manager is already gone). Hence
  // it's safe to use this as a raw pointer.
  GDataFileSystemInterface* file_system_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
