// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

namespace {

// OAuth2 scopes for Drive API.
const char kDriveScope[] = "https://www.googleapis.com/auth/drive";
const char kDriveAppsReadonlyScope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";

scoped_ptr<ResourceList> ParseResourceListOnBlockingPool(
    scoped_ptr<base::Value> value, GDataErrorCode* error) {
  if (!value) {
    // JSON value is not available.
    return scoped_ptr<ResourceList>();
  }

  // Parse the value into ResourceList via ChangeList.
  // If failed, set (i.e. overwrite) the error flag and return immediately.
  scoped_ptr<ChangeList> change_list(ChangeList::CreateFrom(*value));
  if (!change_list) {
    *error = GDATA_PARSE_ERROR;
    return scoped_ptr<ResourceList>();
  }

  scoped_ptr<ResourceList> resource_list =
      ResourceList::CreateFromChangeList(*change_list);
  if (!resource_list) {
    *error = GDATA_PARSE_ERROR;
    return scoped_ptr<ResourceList>();
  }

  // Pass the result to the params, so that DidParseResourceListOnBlockingPool
  // defined below can process it.
  return resource_list.Pass();
}

// Callback invoked when the parsing of resource list is completed,
// regardless whether it is succeeded or not.
void DidParseResourceListOnBlockingPool(
    const GetResourceListCallback& callback,
    GDataErrorCode* error,
    scoped_ptr<ResourceList> resource_list) {
  callback.Run(*error, resource_list.Pass());
}

// Sends a task to parse the JSON value into ResourceList on blocking pool,
// with a callback which is called when the task is done.
void ParseResourceListOnBlockingPoolAndRun(
    const GetResourceListCallback& callback,
    GDataErrorCode in_error,
    scoped_ptr<base::Value> value) {
  // Note that the error value may be overwritten in
  // ParseResoruceListOnBlockingPool before used in
  // DidParseResourceListOnBlockingPool.
  GDataErrorCode* error = new GDataErrorCode(in_error);

  PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ParseResourceListOnBlockingPool,
                 base::Passed(&value), error),
      base::Bind(&DidParseResourceListOnBlockingPool,
                 callback, base::Owned(error)));
}

// Parses the JSON value to ResourceEntry runs |callback|.
void ParseResourceEntryAndRun(
    const GetResourceEntryCallback& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  // Parsing FileResource is cheap enough to do on UI thread.
  scoped_ptr<FileResource> file_resource = FileResource::CreateFrom(*value);
  if (!file_resource) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<ResourceEntry>());
    return;
  }

  // Converting to ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> entry =
      ResourceEntry::CreateFromFileResource(*file_resource);
  if (!entry) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(error, entry.Pass());
}

// Parses the AboutResource value to AccountMetadataFeed and runs |callback|
// on the UI thread once parsing is done.
void ParseAccountMetadataAndRun(
    const GetAccountMetadataCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AboutResource> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<AccountMetadataFeed>());
    return;
  }

  // TODO(satorux): Convert AboutResource to AccountMetadataFeed.
  // For now just returning an error. crbug.com/165621
  callback.Run(GDATA_PARSE_ERROR, scoped_ptr<AccountMetadataFeed>());
}

// Parses the JSON value to AppList runs |callback| on the UI thread
// once parsing is done.
void ParseAppListAndRun(const google_apis::GetAppListCallback& callback,
                        google_apis::GDataErrorCode error,
                        scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<google_apis::AppList>());
    return;
  }

  // Parsing AppList is cheap enough to do on UI thread.
  scoped_ptr<google_apis::AppList> app_list =
      google_apis::AppList::CreateFrom(*value);
  if (!app_list) {
    callback.Run(google_apis::GDATA_PARSE_ERROR,
                 scoped_ptr<google_apis::AppList>());
    return;
  }

  callback.Run(error, app_list.Pass());
}

// The resource ID for the root directory for Drive API is defined in the spec:
// https://developers.google.com/drive/folder
const char kDriveApiRootDirectoryResourceId[] = "root";

}  // namespace

DriveAPIService::DriveAPIService(
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& base_url,
    const std::string& custom_user_agent)
    : url_request_context_getter_(url_request_context_getter),
      profile_(NULL),
      runner_(NULL),
      url_generator_(base_url),
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
  runner_.reset(new OperationRunner(profile,
                                    url_request_context_getter_,
                                    scopes,
                                    custom_user_agent_));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
  runner_->operation_registry()->AddObserver(this);
}

void DriveAPIService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(DriveServiceObserver* observer) {
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

bool DriveAPIService::CancelForFilePath(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->CancelForFilePath(file_path);
}

OperationProgressStatusList DriveAPIService::GetProgressStatusList() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->GetProgressStatusList();
}

std::string DriveAPIService::GetRootResourceId() const {
  return kDriveApiRootDirectoryResourceId;
}

void DriveAPIService::GetResourceList(
    const GURL& url,
    int64 start_changestamp,
    const std::string& search_query,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
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
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetFilelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          url,
          search_query,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::GetChangelist(
    const GURL& url,
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetChangelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          url,
          start_changestamp,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(new GetFileOperation(
      operation_registry(),
      url_request_context_getter_,
      url_generator_,
      resource_id,
      base::Bind(&ParseResourceEntryAndRun, callback)));
}

void DriveAPIService::GetAccountMetadata(
    const GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAboutOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAccountMetadataAndRun, callback)));
}

void DriveAPIService::GetAboutResource(
    const GetAboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAboutOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          callback));
}

void DriveAPIService::GetAppList(const GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(new GetApplistOperation(
      operation_registry(),
      url_request_context_getter_,
      url_generator_,
      base::Bind(&ParseAppListAndRun, callback)));
}

void DriveAPIService::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  runner_->StartOperationWithRetry(
      new DownloadFileOperation(operation_registry(),
                                url_request_context_getter_,
                                download_action_callback,
                                get_content_callback,
                                download_url,
                                virtual_path,
                                local_cache_path));
}

void DriveAPIService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(new drive::TrashResourceOperation(
      operation_registry(),
      url_request_context_getter_,
      url_generator_,
      resource_id,
      callback));
}

void DriveAPIService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::CreateDirectoryOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          parent_resource_id,
          directory_name,
          base::Bind(&ParseResourceEntryAndRun, callback)));
}

void DriveAPIService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::RenameResourceOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          resource_id,
          new_name,
          callback));
}

void DriveAPIService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::InsertResourceOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          parent_resource_id,
          resource_id,
          callback));
}

void DriveAPIService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::DeleteResourceOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          parent_resource_id,
          resource_id,
          callback));
}

void DriveAPIService::InitiateUploadNewFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::InitiateUploadNewFileOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          drive_file_path,
          content_type,
          content_length,
          parent_resource_id,
          title,
          callback));
}

void DriveAPIService::InitiateUploadExistingFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(hidehiko): Implement this.
  NOTREACHED();
}

void DriveAPIService::ResumeUpload(
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const scoped_refptr<net::IOBuffer>& buf,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::GetUploadStatus(
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(hidehiko): Implement this.
  NOTREACHED();
}

void DriveAPIService::AuthorizeApp(
    const GURL& edit_url,
    const std::string& app_ids,
    const AuthorizeAppCallback& callback) {
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

void DriveAPIService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearAccessToken();
}

void DriveAPIService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearRefreshToken();
}

OperationRegistry* DriveAPIService::operation_registry() const {
  return runner_->operation_registry();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanStartOperation()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToPerformOperations());
  } else if (!HasRefreshToken()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnRefreshTokenInvalid());
  }
}

void DriveAPIService::OnProgressUpdate(
    const OperationProgressStatusList& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      DriveServiceObserver, observers_, OnProgressUpdate(list));
}

}  // namespace google_apis
