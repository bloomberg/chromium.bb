// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FILE_SYSTEM_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace google_apis {

class AboutResource;
class ResourceEntry;
class ResourceList;

}  // namespace google_apis

namespace drive {

class DriveServiceInterface;
class FileSystemObserver;
class ResourceEntry;

namespace test_util {

// This class implements a fake FileSystem which acts like a real Drive
// file system with FakeDriveService, for testing purpose.
// Note that this class doesn't support "caching" at the moment, so the number
// of interactions to the FakeDriveService may be bigger than the real
// implementation.
// Currently most methods are empty (not implemented).
class FakeFileSystem : public FileSystemInterface {
 public:
  explicit FakeFileSystem(DriveServiceInterface* drive_service);
  virtual ~FakeFileSystem();

  // Initialization for testing. This can be called instead of Initialize
  // for testing purpose. Returns true for success.
  bool InitializeForTesting();

  // FileSystemInterface Overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenMode open_mode,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const base::FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE;
  virtual void TouchFile(const base::FilePath& file_path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void TruncateFile(const base::FilePath& file_path,
                            int64 length,
                            const FileOperationCallback& callback) OVERRIDE;
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) OVERRIDE;
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByPathForSaving(const base::FilePath& file_path,
                                      const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) OVERRIDE;
  virtual void GetResourceEntryByPath(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const base::FilePath& file_path,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      const GURL& next_url,
                      const SearchCallback& callback) OVERRIDE;
  virtual void SearchMetadata(const std::string& query,
                              int options,
                              int at_most_num_matches,
                              const SearchMetadataCallback& callback) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void GetShareUrl(
      const base::FilePath& file_path,
      const GURL& embed_origin,
      const GetShareUrlCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsMounted(
      const base::FilePath& drive_file_path,
      const MarkMountedCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetCacheEntryByResourceId(
      const std::string& resource_id,
      const GetCacheEntryCallback& callback) OVERRIDE;
  virtual void Reload() OVERRIDE;

 private:
  // Helper of GetResourceEntryById.
  void GetResourceEntryByIdAfterGetResourceEntry(
      const GetResourceEntryCallback& callback,
      google_apis::GDataErrorCode error_in,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Helpers of GetFileContentByPath.
  // How the method works:
  // 1) Gets ResourceEntry of the path.
  // 2) Look at if there is a cache file or not. If found return it.
  // 3) Otherwise start DownloadFile.
  // 4) Runs the |completion_callback| upon the download completion.
  void GetFileContentByPathAfterGetResourceEntry(
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);
  void GetFileContentByPathAfterGetWapiResourceEntry(
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::ResourceEntry> gdata_entry);
  void GetFileContentByPathAfterDownloadFile(
      const FileOperationCallback& completion_callback,
      google_apis::GDataErrorCode gdata_error,
      const base::FilePath& temp_file);

  // Helpers of GetResourceEntryByPath.
  // How the method works:
  // 1) If the path is root, gets AboutResrouce from the drive service
  //    and create ResourceEntry.
  // 2-1) Otherwise, gets the parent's ResourceEntry by recursive call.
  // 2-2) Then, gets the resource list by restricting the parent with its id.
  // 2-3) Search the results based on title, and return the ResourceEntry.
  // Note that adding suffix (e.g. " (2)") for files sharing a same name is
  // not supported in FakeFileSystem. Thus, even if the server has
  // files sharing the same name under a directory, the second (or later)
  // file cannot be taken with the suffixed name.
  void GetResourceEntryByPathAfterGetAboutResource(
      const GetResourceEntryCallback& callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::AboutResource> about_resource);
  void GetResourceEntryByPathAfterGetParentEntryInfo(
      const base::FilePath& base_name,
      const GetResourceEntryCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> parent_entry);
  void GetResourceEntryByPathAfterGetResourceList(
      const base::FilePath& base_name,
      const GetResourceEntryCallback& callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  DriveServiceInterface* drive_service_;  // Not owned.
  base::ScopedTempDir cache_dir_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeFileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeFileSystem);
};

}  // namespace test_util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FILE_SYSTEM_H_
