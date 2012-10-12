// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class FilePath;
class GURL;

namespace gdata {

class DriveCache;
class DriveEntryProto;
class DriveFileSystem;
class DriveServiceInterface;

namespace file_system {

// This class encapsulates the drive Remove function.  It is resposible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class RemoveOperation {
 public:
  RemoveOperation(DriveServiceInterface* drive_service,
                      DriveFileSystem* file_system,
                      DriveCache* cache);
  virtual ~RemoveOperation();

  // Perform the remove operation on the file at drive path |file_path|.
  // Invokes |callback| when finished with the result of the operation.
  virtual void Remove(const FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback);

 private:
  // Part of Remove(). Called after GetEntryInfoByPath() is complete.
  // |callback| must not be null.
  void RemoveAfterGetEntryInfo(
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Callback for DriveServiceInterface::DeleteDocument. Removes the entry with
  // |resource_id| from the local snapshot of the filesystem and the cache.
  // |document_url| is unused. |callback| must not be null.
  void RemoveResourceLocally(
      const FileOperationCallback& callback,
      const std::string& resource_id,
      GDataErrorCode status,
      const GURL& /* document_url */);

  // Sends notification for directory changes. Notifies of directory changes,
  // and runs |callback| with |error|. |callback| may be null.
  void NotifyDirectoryChanged(
      const FileOperationCallback& callback,
      DriveFileError error,
      const FilePath& directory_path);

  DriveServiceInterface* drive_service_;
  DriveFileSystem* file_system_;
  DriveCache* cache_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RemoveOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoveOperation);
};

}  // namespace file_system
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
