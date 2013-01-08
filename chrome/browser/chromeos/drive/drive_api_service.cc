// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// OAuth2 scopes for Drive API.
const char kDriveScope[] = "https://www.googleapis.com/auth/drive";
const char kDriveAppsReadonlyScope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";

// Parses the JSON value into ResourceList and runs |callback|.
void ParseResourceListAndRun(
    const google_apis::GetResourceListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  if (!value) {
    callback.Run(error, scoped_ptr<google_apis::ResourceList>());
    return;
  }

  // TODO(satorux): Parse the JSON value on a blocking pool. crbug.com/165088
  scoped_ptr<google_apis::ChangeList> change_list(
      google_apis::ChangeList::CreateFrom(*value));
  if (!change_list) {
    callback.Run(google_apis::GDATA_PARSE_ERROR,
                 scoped_ptr<google_apis::ResourceList>());
    return;
  }

  // TODO(satorux): Do the conversion on a blocking pool too. crbug.com/165088
  scoped_ptr<google_apis::ResourceList> resource_list =
      google_apis::ResourceList::CreateFromChangeList(*change_list);
  if (!resource_list) {
    callback.Run(google_apis::GDATA_PARSE_ERROR,
                 scoped_ptr<google_apis::ResourceList>());
    return;
  }

  callback.Run(error, resource_list.Pass());
}

// Parses the JSON value to ResourceEntry runs |callback|.
void ParseResourceEntryAndRun(
    const google_apis::GetResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!value) {
    callback.Run(error, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  // Parsing FileResource is cheap enough to do on UI thread.
  scoped_ptr<google_apis::FileResource> file_resource =
      google_apis::FileResource::CreateFrom(*value);
  if (!file_resource) {
    callback.Run(google_apis::GDATA_PARSE_ERROR,
                 scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  // Converting to ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<google_apis::ResourceEntry> entry =
      google_apis::ResourceEntry::CreateFromFileResource(*file_resource);
  if (!entry) {
    callback.Run(google_apis::GDATA_PARSE_ERROR,
                 scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  callback.Run(error, entry.Pass());
}

// Parses the JSON value to AccountMetadataFeed on the blocking pool and runs
// |callback| on the UI thread once parsing is done.
void ParseAccounetMetadataAndRun(
    const google_apis::GetAccountMetadataCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<google_apis::AccountMetadataFeed>());
    return;
  }

  // Parsing AboutResource is cheap enough to do on UI thread.
  scoped_ptr<google_apis::AboutResource> about_resource =
      google_apis::AboutResource::CreateFrom(*value);

  // TODO(satorux): Convert AboutResource to AccountMetadataFeed.
  // For now just returning an error. crbug.com/165621
  callback.Run(google_apis::GDATA_PARSE_ERROR,
               scoped_ptr<google_apis::AccountMetadataFeed>());
}

}  // namespace

DriveAPIService::DriveAPIService(
    net::URLRequestContextGetter* url_request_context_getter,
    const std::string& custom_user_agent)
    : url_request_context_getter_(url_request_context_getter),
      profile_(NULL),
      runner_(NULL),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveAPIService::~DriveAPIService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get()) {
    runner_->operation_registry()->RemoveObserver(this);
    runner_->auth_service()->RemoveObserver(this);
  }
}

void DriveAPIService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;

  std::vector<std::string> scopes;
  scopes.push_back(kDriveScope);
  scopes.push_back(kDriveAppsReadonlyScope);
  runner_.reset(
      new google_apis::OperationRunner(profile,
                                       url_request_context_getter_,
                                       scopes,
                                       custom_user_agent_));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
  runner_->operation_registry()->AddObserver(this);
}

void DriveAPIService::AddObserver(google_apis::DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(
    google_apis::DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveAPIService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

void DriveAPIService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

bool DriveAPIService::CancelForFilePath(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->CancelForFilePath(file_path);
}

google_apis::OperationProgressStatusList
DriveAPIService::GetProgressStatusList() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->GetProgressStatusList();
}

void DriveAPIService::GetResourceList(
    const GURL& url,
    int64 start_changestamp,
    const std::string& search_query,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (search_query.empty())
    GetChangelist(url, start_changestamp, callback);
  else
    GetFilelist(url, search_query, callback);

  return;
  // TODO(kochi): Implement !directory_resource_id.empty() case.
  NOTREACHED();
}

void DriveAPIService::GetFilelist(
    const GURL& url,
    const std::string& search_query,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new google_apis::GetFilelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          url,
          search_query,
          base::Bind(&ParseResourceListAndRun, callback)));
}

void DriveAPIService::GetChangelist(
    const GURL& url,
    int64 start_changestamp,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new google_apis::GetChangelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          url,
          start_changestamp,
          base::Bind(&ParseResourceListAndRun, callback)));
}

void DriveAPIService::GetResourceEntry(
    const std::string& resource_id,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(new google_apis::GetFileOperation(
      operation_registry(),
      url_request_context_getter_,
      url_generator_,
      resource_id,
      base::Bind(&ParseResourceEntryAndRun, callback)));
}

void DriveAPIService::GetAccountMetadata(
    const google_apis::GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new google_apis::GetAboutOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAccounetMetadataAndRun, callback)));
}

void DriveAPIService::GetApplicationInfo(
    const google_apis::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new google_apis::GetApplistOperation(operation_registry(),
                                           url_request_context_getter_,
                                           url_generator_,
                                           callback));
}

void DriveAPIService::DownloadHostedDocument(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& content_url,
    google_apis::DocumentExportFormat format,
    const google_apis::DownloadActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::DeleteResource(
    const GURL& edit_url,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::AddNewDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::CopyHostedDocument(
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::RenameResource(
    const GURL& edit_url,
    const FilePath::StringType& new_name,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& edit_url,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::InitiateUpload(
    const google_apis::InitiateUploadParams& params,
    const google_apis::InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::ResumeUpload(
    const google_apis::ResumeUploadParams& params,
    const google_apis::ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::AuthorizeApp(
    const GURL& edit_url,
    const std::string& app_ids,
    const google_apis::AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

bool DriveAPIService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasAccessToken();
}

bool DriveAPIService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasRefreshToken();
}

google_apis::OperationRegistry* DriveAPIService::operation_registry() const {
  return runner_->operation_registry();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanStartOperation()) {
    FOR_EACH_OBSERVER(
        google_apis::DriveServiceObserver, observers_,
        OnReadyToPerformOperations());
  }
}

void DriveAPIService::OnProgressUpdate(
    const google_apis::OperationProgressStatusList& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      google_apis::DriveServiceObserver, observers_, OnProgressUpdate(list));
}

void DriveAPIService::OnAuthenticationFailed(
    google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      google_apis::DriveServiceObserver, observers_,
      OnAuthenticationFailed(error));
}

}  // namespace drive
