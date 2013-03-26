// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

namespace drive {
namespace test_util {

using content::BrowserThread;

FakeDriveFileSystem::FakeDriveFileSystem(
    google_apis::DriveServiceInterface* drive_service)
    : drive_service_(drive_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

FakeDriveFileSystem::~FakeDriveFileSystem() {
}

void FakeDriveFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::AddObserver(DriveFileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::RemoveObserver(DriveFileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::StartInitialFeedFetch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::StartPolling() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::StopPolling() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::SetPushNotificationEnabled(bool enabled) {
}

void FakeDriveFileSystem::NotifyFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::NotifyFileSystemToBeUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void FakeDriveFileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::OpenFile(const base::FilePath& file_path,
                                   const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CloseFile(const base::FilePath& file_path,
                                    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Copy(const base::FilePath& src_file_path,
                               const base::FilePath& dest_file_path,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Move(const base::FilePath& src_file_path,
                               const base::FilePath& dest_file_path,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Remove(const base::FilePath& file_path,
                                 bool is_recursive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CreateFile(const base::FilePath& file_path,
                                     bool is_exclusive,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetFileByPath(const base::FilePath& file_path,
                                        const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CancelGetFile(const base::FilePath& drive_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetEntryInfoByPath(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::ReadDirectoryByPath(
    const base::FilePath& file_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::RefreshDirectory(
    const base::FilePath& file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Search(const std::string& search_query,
                                 bool shared_with_me,
                                 const GURL& next_feed,
                                 const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::AddUploadedFile(
    scoped_ptr<google_apis::ResourceEntry> doc_entry,
    const base::FilePath& file_content_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Reload() {
}

// Implementation of GetFilePath.
void FakeDriveFileSystem::GetFilePath(const std::string& resource_id,
                                      const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetAboutResource(
      base::Bind(
          &FakeDriveFileSystem::GetFilePathAfterGetAboutResource,
          weak_ptr_factory_.GetWeakPtr(), resource_id, callback));
}

void FakeDriveFileSystem::GetFilePathAfterGetAboutResource(
    const std::string& resource_id,
    const GetFilePathCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We assume the call always success for test.
  DCHECK_EQ(util::GDataToDriveFileError(error), DRIVE_FILE_OK);
  DCHECK(about_resource);

  GetFilePathInternal(about_resource->root_folder_id(), resource_id,
                      base::FilePath(), callback);
}

void FakeDriveFileSystem::GetFilePathInternal(
    const std::string& root_resource_id,
    const std::string& resource_id,
    const base::FilePath& file_path,
    const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (resource_id == root_resource_id) {
    // Reached to the root. Append the drive root path, and run |callback|.
    callback.Run(util::GetDriveMyDriveRootPath().Append(file_path));
    return;
  }

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetFilePathAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(),
          root_resource_id, file_path, callback));
}

void FakeDriveFileSystem::GetFilePathAfterGetResourceEntry(
    const std::string& root_resource_id,
    const base::FilePath& remaining_file_path,
    const GetFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We assume the call always success for test.
  DCHECK_EQ(util::GDataToDriveFileError(error_in), DRIVE_FILE_OK);
  DCHECK(resource_entry);

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);
  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(entry_proto.base_name()).Append(
          remaining_file_path);

  GetFilePathInternal(root_resource_id, entry_proto.parent_resource_id(),
                      file_path, callback);
}

// Implementation of GetEntryInfoByResourceId.
void FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry(
    const GetEntryInfoWithFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError error = util::GDataToDriveFileError(error_in);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<DriveEntryProto>());
    return;
  }

  DCHECK(resource_entry);
  scoped_ptr<DriveEntryProto> entry_proto(new DriveEntryProto(
      ConvertResourceEntryToDriveEntryProto(*resource_entry)));

  const std::string parent_resource_id = entry_proto->parent_resource_id();
  GetFilePath(
      parent_resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetFilePath,
          weak_ptr_factory_.GetWeakPtr(),
          callback, error, base::Passed(&entry_proto)));
}

void FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetFilePath(
    const GetEntryInfoWithFilePathCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto,
    const base::FilePath& parent_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FilePath file_path = parent_file_path.Append(
      base::FilePath::FromUTF8Unsafe(entry_proto->base_name()));
  callback.Run(error, file_path, entry_proto.Pass());
}

}  // namespace test_util
}  // namespace drive
