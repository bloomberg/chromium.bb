// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/dummy_drive_service.h"

namespace google_apis {

DummyDriveService::DummyDriveService() {}

DummyDriveService::~DummyDriveService() {}

void DummyDriveService::Initialize(Profile* profile) {}

void DummyDriveService::AddObserver(DriveServiceObserver* observer) {}

void DummyDriveService::RemoveObserver(DriveServiceObserver* observer) {}

bool DummyDriveService::CanSendRequest() const { return true; }

void DummyDriveService::CancelAll() {}

bool DummyDriveService::CancelForFilePath(const base::FilePath& file_path) {
  return true;
}

std::string DummyDriveService::CanonicalizeResourceId(
    const std::string& resource_id) const {
  return resource_id;
}

bool DummyDriveService::HasAccessToken() const { return true; }

bool DummyDriveService::HasRefreshToken() const { return true; }

void DummyDriveService::ClearAccessToken() { }

void DummyDriveService::ClearRefreshToken() { }

std::string DummyDriveService::GetRootResourceId() const {
  return "dummy_root";
}

CancelCallback DummyDriveService::GetAllResourceList(
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::Search(
    const std::string& search_query,
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetChangeList(
    int64 start_changestamp,
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::ContinueGetResourceList(
    const GURL& override_url,
    const GetResourceListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetAboutResource(
    const GetAboutResourceCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetAppList(
    const GetAppListCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) { return CancelCallback(); }

CancelCallback DummyDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::TouchResource(
    const std::string& resource_id,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const GetResourceEntryCallback& callback) { return CancelCallback(); }

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
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::InitiateUploadNewFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::InitiateUploadExistingFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::ResumeUpload(
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) { return CancelCallback(); }

CancelCallback DummyDriveService::GetUploadStatus(
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) { return CancelCallback(); }

CancelCallback DummyDriveService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) { return CancelCallback(); }

}  // namespace google_apis
