// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/dummy_drive_service.h"

#include "base/bind.h"

using google_apis::AboutResourceCallback;
using google_apis::AppListCallback;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::CancelCallback;
using google_apis::ChangeListCallback;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileListCallback;
using google_apis::FileResourceCallback;
using google_apis::GetContentCallback;
using google_apis::GetShareUrlCallback;
using google_apis::InitiateUploadCallback;
using google_apis::ProgressCallback;
using google_apis::drive::UploadRangeCallback;

namespace drive {

DummyDriveService::DummyDriveService() {}

DummyDriveService::~DummyDriveService() {}

void DummyDriveService::Initialize(const std::string& account_id) {}

void DummyDriveService::AddObserver(DriveServiceObserver* observer) {}

void DummyDriveService::RemoveObserver(DriveServiceObserver* observer) {}

bool DummyDriveService::CanSendRequest() const { return true; }

bool DummyDriveService::HasAccessToken() const { return true; }

void DummyDriveService::RequestAccessToken(const AuthStatusCallback& callback) {
  callback.Run(google_apis::HTTP_NOT_MODIFIED, "fake_access_token");
}

bool DummyDriveService::HasRefreshToken() const { return true; }

void DummyDriveService::ClearAccessToken() { }

void DummyDriveService::ClearRefreshToken() { }

std::string DummyDriveService::GetRootResourceId() const {
  return "dummy_root";
}

CancelCallback DummyDriveService::GetAllFileList(
    const FileListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const FileListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::Search(
    const std::string& search_query,
    const FileListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const FileListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetChangeList(
    int64 start_changestamp,
    const ChangeListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetRemainingChangeList(
    const GURL& next_link,
    const ChangeListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetRemainingFileList(
    const GURL& next_link,
    const FileListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetFileResource(
    const std::string& resource_id,
    const FileResourceCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetShareUrl(
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetShareUrlCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetAboutResource(
    const AboutResourceCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetAppList(
    const AppListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::TrashResource(
    const std::string& resource_id,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) { return CancelCallback(); }

CancelCallback DummyDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const FileResourceCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::FileResourceCallback& callback) {
  return CancelCallback();
}

CancelCallback DummyDriveService::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const FileResourceCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadNewFileOptions& options,
    const InitiateUploadCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const InitiateUploadExistingFileOptions& options,
    const InitiateUploadCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::ResumeUpload(
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::UninstallApp(
    const std::string& app_id,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const EntryActionCallback& callback) { return CancelCallback(); }

}  // namespace drive
