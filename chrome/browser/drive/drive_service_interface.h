// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_DRIVE_DRIVE_SERVICE_INTERFACE_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/drive/auth_service_interface.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace base {
class Time;
}

namespace drive {

// Function which converts the given resource ID into the desired format.
typedef base::Callback<std::string(
    const std::string& resource_id)> ResourceIdCanonicalizer;

// Observer interface for DriveServiceInterface.
class DriveServiceObserver {
 public:
  // Triggered when the service gets ready to send requests.
  virtual void OnReadyToSendRequests() {}

  // Called when the refresh token was found to be invalid.
  virtual void OnRefreshTokenInvalid() {}

 protected:
  virtual ~DriveServiceObserver() {}
};

// This defines an interface for sharing by DriveService and MockDriveService
// so that we can do testing of clients of DriveService.
//
// All functions must be called on UI thread. DriveService is built on top of
// URLFetcher that runs on UI thread.
class DriveServiceInterface {
 public:
  // Optional parameters for AddNewDirectory().
  struct AddNewDirectoryOptions {
    AddNewDirectoryOptions();
    ~AddNewDirectoryOptions();

    // modified_date of the directory.
    // Pass the null Time if you are not interested in setting this property.
    base::Time modified_date;

    // last_viewed_by_me_date of the directory.
    // Pass the null Time if you are not interested in setting this property.
    base::Time last_viewed_by_me_date;
  };

  // Optional parameters for InitiateUploadNewFile().
  struct InitiateUploadNewFileOptions {
    InitiateUploadNewFileOptions();
    ~InitiateUploadNewFileOptions();

    // modified_date of the file.
    // Pass the null Time if you are not interested in setting this property.
    base::Time modified_date;

    // last_viewed_by_me_date of the file.
    // Pass the null Time if you are not interested in setting this property.
    base::Time last_viewed_by_me_date;
  };

  // Optional parameters for InitiateUploadExistingFile().
  struct InitiateUploadExistingFileOptions {
    InitiateUploadExistingFileOptions();
    ~InitiateUploadExistingFileOptions();

    // Expected ETag of the file. UPLOAD_ERROR_CONFLICT error is generated when
    // matching fails.
    // Pass the empty string to disable this behavior.
    std::string etag;

    // New parent of the file.
    // Pass the empty string to keep the property unchanged.
    std::string parent_resource_id;

    // New title of the file.
    // Pass the empty string to keep the property unchanged.
    std::string title;

    // New modified_date of the file.
    // Pass the null Time if you are not interested in setting this property.
    base::Time modified_date;

    // New last_viewed_by_me_date of the file.
    // Pass the null Time if you are not interested in setting this property.
    base::Time last_viewed_by_me_date;
  };

  virtual ~DriveServiceInterface() {}

  // Common service:

  // Initializes the documents service with |account_id|.
  virtual void Initialize(const std::string& account_id) = 0;

  // Adds an observer.
  virtual void AddObserver(DriveServiceObserver* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(DriveServiceObserver* observer) = 0;

  // True if ready to send requests.
  virtual bool CanSendRequest() const = 0;

  // Returns a function which converts the given resource ID into the desired
  // format.
  virtual ResourceIdCanonicalizer GetResourceIdCanonicalizer() const = 0;

  // Authentication service:

  // True if OAuth2 access token is retrieved and believed to be fresh.
  virtual bool HasAccessToken() const = 0;

  // Gets the cached OAuth2 access token or if empty, then fetches a new one.
  virtual void RequestAccessToken(
      const google_apis::AuthStatusCallback& callback) = 0;

  // True if OAuth2 refresh token is present.
  virtual bool HasRefreshToken() const = 0;

  // Clears OAuth2 access token.
  virtual void ClearAccessToken() = 0;

  // Clears OAuth2 refresh token.
  virtual void ClearRefreshToken() = 0;

  // Document access:

  // Returns the resource id for the root directory.
  virtual std::string GetRootResourceId() const = 0;

  // Fetches a file list of the account. |callback| will be called upon
  // completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetAllFileList(
      const google_apis::FileListCallback& callback) = 0;

  // Fetches a file list in the directory with |directory_resource_id|.
  // |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |directory_resource_id| must not be empty.
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) = 0;

  // Searches the resources for the |search_query| from all the user's
  // resources. |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |search_query| must not be empty.
  // |callback| must not be null.
  virtual google_apis::CancelCallback Search(
      const std::string& search_query,
      const google_apis::FileListCallback& callback) = 0;

  // Searches the resources with the |title|.
  // |directory_resource_id| is an optional parameter. If it is empty,
  // the search target is all the existing resources. Otherwise, it is
  // the resources directly under the directory with |directory_resource_id|.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |title| must not be empty, and |callback| must not be null.
  virtual google_apis::CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) = 0;

  // Fetches change list since |start_changestamp|. |callback| will be
  // called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingChangeList.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetChangeList(
      int64 start_changestamp,
      const google_apis::ChangeListCallback& callback) = 0;

  // The result of GetChangeList() may be paged.
  // In such a case, a next link to fetch remaining result is returned.
  // The page token can be used for this method. |callback| will be called upon
  // completion.
  //
  // |next_link| must not be empty. |callback| must not be null.
  virtual google_apis::CancelCallback GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback) = 0;

  // The result of GetAllFileList(), GetFileListInDirectory(), Search()
  // and SearchByTitle() may be paged. In such a case, a next link to fetch
  // remaining result is returned. The page token can be used for this method.
  // |callback| will be called upon completion.
  //
  // |next_link| must not be empty. |callback| must not be null.
  virtual google_apis::CancelCallback GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback) = 0;

  // Fetches single entry metadata from server. The entry's file id equals
  // |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback) = 0;

  // Fetches an url for the sharing dialog for a single entry with id
  // |resource_id|, to be embedded in a webview or an iframe with origin
  // |embed_origin|. The url is returned via |callback| with results on the
  // calling thread. |callback| must not be null.
  virtual google_apis::CancelCallback GetShareUrl(
      const std::string& resource_id,
      const GURL& embed_origin,
      const google_apis::GetShareUrlCallback& callback) = 0;

  // Gets the about resource information from the server.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetAboutResource(
      const google_apis::AboutResourceCallback& callback) = 0;

  // Gets the application information from the server.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetAppList(
      const google_apis::AppListCallback& callback) = 0;

  // Permanently deletes a resource identified by its |resource_id|.
  // If |etag| is not empty and did not match, the deletion fails with
  // HTTP_PRECONDITION error.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback) = 0;

  // Trashes a resource identified by its |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback TrashResource(
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) = 0;

  // Makes a copy of a resource with |resource_id|.
  // The new resource will be put under a directory with |parent_resource_id|,
  // and it'll be named |new_title|.
  // If |last_modified| is not null, the modified date of the resource on the
  // server will be set to the date.
  // This request is supported only on DriveAPIService, because GData WAPI
  // doesn't support the function unfortunately.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const google_apis::FileResourceCallback& callback) = 0;

  // Updates a resource with |resource_id| to the directory of
  // |parent_resource_id| with renaming to |new_title|.
  // If |last_modified| or |last_accessed| is not null, the modified/accessed
  // date of the resource on the server will be set to the date.
  // This request is supported only on DriveAPIService, because GData WAPI
  // doesn't support the function unfortunately.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::FileResourceCallback& callback) = 0;

  // Renames a document or collection identified by its |resource_id|
  // to the UTF-8 encoded |new_title|. Upon completion,
  // invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback RenameResource(
      const std::string& resource_id,
      const std::string& new_title,
      const google_apis::EntryActionCallback& callback) = 0;

  // Adds a resource (document, file, or collection) identified by its
  // |resource_id| to a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) = 0;

  // Removes a resource (document, file, collection) identified by its
  // |resource_id| from a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) = 0;

  // Adds new collection with |directory_title| under parent directory
  // identified with |parent_resource_id|. |parent_resource_id| can be the
  // value returned by GetRootResourceId to represent the root directory.
  // Upon completion, invokes |callback| and passes newly created entry on
  // the calling thread.
  // This function cannot be named as "CreateDirectory" as it conflicts with
  // a macro on Windows.
  // |callback| must not be null.
  virtual google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) = 0;

  // Downloads a file with |resourced_id|. The downloaded file will
  // be stored at |local_cache_path| location. Upon completion, invokes
  // |download_action_callback| with results on the calling thread.
  // If |get_content_callback| is not empty,
  // URLFetcherDelegate::OnURLFetchDownloadData will be called, which will in
  // turn invoke |get_content_callback| on the calling thread.
  // If |progress_callback| is not empty, it is invoked periodically when
  // the download made some progress.
  //
  // |download_action_callback| must not be null.
  // |get_content_callback| and |progress_callback| may be null.
  virtual google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) = 0;

  // Initiates uploading of a new document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual google_apis::CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadNewFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) = 0;

  // Initiates uploading of an existing document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual google_apis::CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const InitiateUploadExistingFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) = 0;

  // Resumes uploading of a document/file on the calling thread.
  // |callback| must not be null. |progress_callback| may be null.
  virtual google_apis::CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const google_apis::drive::UploadRangeCallback& callback,
      const google_apis::ProgressCallback& progress_callback) = 0;

  // Gets the current status of the uploading to |upload_url| from the server.
  // |drive_file_path| and |content_length| should be set to the same value
  // which is used for ResumeUpload.
  // |callback| must not be null.
  virtual google_apis::CancelCallback GetUploadStatus(
      const GURL& upload_url,
      int64 content_length,
      const google_apis::drive::UploadRangeCallback& callback) = 0;

  // Authorizes a Drive app with the id |app_id| to open the given file.
  // Upon completion, invokes |callback| with the link to open the file with
  // the provided app. |callback| must not be null.
  virtual google_apis::CancelCallback AuthorizeApp(
      const std::string& resource_id,
      const std::string& app_id,
      const google_apis::AuthorizeAppCallback& callback) = 0;

  // Uninstalls a Drive app with the id |app_id|. |callback| must not be null.
  virtual google_apis::CancelCallback UninstallApp(
      const std::string& app_id,
      const google_apis::EntryActionCallback& callback) = 0;

  // Authorizes the account |email| to access |resource_id| as a |role|.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallback AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      const google_apis::EntryActionCallback& callback) = 0;
};

}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_SERVICE_INTERFACE_H_
