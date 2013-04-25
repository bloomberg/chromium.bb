// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
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

// Expected max number of files resources in a http request.
// Be careful not to use something too small because it might overload the
// server. Be careful not to use something too large because it takes longer
// time to fetch the result without UI response.
const int kMaxNumFilesResourcePerRequest = 500;
const int kMaxNumFilesResourcePerRequestForSearch = 50;

scoped_ptr<ResourceList> ParseChangeListJsonToResourceList(
    scoped_ptr<base::Value> value) {
  scoped_ptr<ChangeList> change_list(ChangeList::CreateFrom(*value));
  if (!change_list) {
    return scoped_ptr<ResourceList>();
  }

  return ResourceList::CreateFromChangeList(*change_list);
}

scoped_ptr<ResourceList> ParseFileListJsonToResourceList(
    scoped_ptr<base::Value> value) {
  scoped_ptr<FileList> file_list(FileList::CreateFrom(*value));
  if (!file_list) {
    return scoped_ptr<ResourceList>();
  }

  return ResourceList::CreateFromFileList(*file_list);
}

// Parses JSON value representing either ChangeList or FileList into
// ResourceList.
scoped_ptr<ResourceList> ParseResourceListOnBlockingPool(
    scoped_ptr<base::Value> value) {
  DCHECK(value);

  // Dispatch the parsing based on kind field.
  if (ChangeList::HasChangeListKind(*value)) {
    return ParseChangeListJsonToResourceList(value.Pass());
  }
  if (FileList::HasFileListKind(*value)) {
    return ParseFileListJsonToResourceList(value.Pass());
  }

  // The value type is unknown, so give up to parse and return an error.
  return scoped_ptr<ResourceList>();
}

// Callback invoked when the parsing of resource list is completed,
// regardless whether it is succeeded or not.
void DidParseResourceListOnBlockingPool(
    const GetResourceListCallback& callback,
    scoped_ptr<ResourceList> resource_list) {
  GDataErrorCode error = resource_list ? HTTP_SUCCESS : GDATA_PARSE_ERROR;
  callback.Run(error, resource_list.Pass());
}

// Sends a task to parse the JSON value into ResourceList on blocking pool,
// with a callback which is called when the task is done.
void ParseResourceListOnBlockingPoolAndRun(
    const GetResourceListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != HTTP_SUCCESS) {
    // An error occurs, so run callback immediately.
    callback.Run(error, scoped_ptr<ResourceList>());
    return;
  }

  PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ParseResourceListOnBlockingPool, base::Passed(&value)),
      base::Bind(&DidParseResourceListOnBlockingPool, callback));
}

// Parses the FileResource value to ResourceEntry and runs |callback| on the
// UI thread.
void ParseResourceEntryAndRun(
    const GetResourceEntryCallback& callback,
    GDataErrorCode error,
    scoped_ptr<FileResource> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  // Converting to ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> entry =
      ResourceEntry::CreateFromFileResource(*value);
  if (!entry) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(error, entry.Pass());
}

// Parses the AboutResource value to AccountMetadata and runs |callback|
// on the UI thread once parsing is done.
void ParseAccountMetadataAndRun(
    const GetAccountMetadataCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AboutResource> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<AccountMetadata>());
    return;
  }

  // TODO(satorux): Convert AboutResource to AccountMetadata.
  // For now just returning an error. crbug.com/165621
  callback.Run(GDATA_PARSE_ERROR, scoped_ptr<AccountMetadata>());
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

// Parses the FileResource value to ResourceEntry for upload range operation,
// and runs |callback| on the UI thread.
void ParseResourceEntryForUploadRangeAndRun(
    const UploadRangeCallback& callback,
    const UploadRangeResponse& response,
    scoped_ptr<FileResource> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(response, scoped_ptr<ResourceEntry>());
    return;
  }

  // Converting to ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> entry =
      ResourceEntry::CreateFromFileResource(*value);
  if (!entry) {
    callback.Run(UploadRangeResponse(GDATA_PARSE_ERROR,
                                     response.start_position_received,
                                     response.end_position_received),
                 scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(response, entry.Pass());
}

// The resource ID for the root directory for Drive API is defined in the spec:
// https://developers.google.com/drive/folder
const char kDriveApiRootDirectoryResourceId[] = "root";

}  // namespace

DriveAPIService::DriveAPIService(
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& base_url,
    const GURL& wapi_base_url,
    const std::string& custom_user_agent)
    : url_request_context_getter_(url_request_context_getter),
      profile_(NULL),
      runner_(NULL),
      url_generator_(base_url),
      wapi_url_generator_(wapi_base_url),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveAPIService::~DriveAPIService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get())
    runner_->auth_service()->RemoveObserver(this);
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

std::string DriveAPIService::GetRootResourceId() const {
  return kDriveApiRootDirectoryResourceId;
}

void DriveAPIService::GetAllResourceList(
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // The simplest way to fetch the all resources list looks files.list method,
  // but it seems impossible to know the returned list's changestamp.
  // Thus, instead, we use changes.list method with includeDeleted=false here.
  // The returned list should contain only resources currently existing.
  runner_->StartOperationWithRetry(
      new GetChangelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          false,  // include deleted
          0,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  // Because children.list method on Drive API v2 returns only the list of
  // children's references, but we need all file resource list.
  // So, here we use files.list method instead, with setting parents query.
  // After the migration from GData WAPI to Drive API v2, we should clean the
  // code up by moving the resposibility to include "parents" in the query
  // to client side.
  // We aren't interested in files in trash in this context, neither.
  runner_->StartOperationWithRetry(
      new GetFilelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::StringPrintf(
              "'%s' in parents and trashed = false",
              drive::util::EscapeQueryStringValue(
                  directory_resource_id).c_str()),
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::Search(const std::string& search_query,
                             const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetFilelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          drive::util::TranslateQuery(search_query),
          kMaxNumFilesResourcePerRequestForSearch,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  std::string query;
  base::StringAppendF(&query, "title = '%s'",
                      drive::util::EscapeQueryStringValue(title).c_str());
  if (!directory_resource_id.empty()) {
    base::StringAppendF(
        &query, " and '%s' in parents",
        drive::util::EscapeQueryStringValue(directory_resource_id).c_str());
  }
  query += " and trashed = false";

  runner_->StartOperationWithRetry(
      new GetFilelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          query,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::GetChangeList(int64 start_changestamp,
                                    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetChangelistOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          true,  // include deleted
          start_changestamp,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun, callback)));
}

void DriveAPIService::ContinueGetResourceList(
    const GURL& override_url,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::ContinueGetFileListOperation(
          operation_registry(),
          url_request_context_getter_,
          override_url,
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
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  runner_->StartOperationWithRetry(
      new DownloadFileOperation(operation_registry(),
                                url_request_context_getter_,
                                download_action_callback,
                                get_content_callback,
                                progress_callback,
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

  runner_->StartOperationWithRetry(
      new drive::CopyResourceOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          resource_id,
          new_name,
          base::Bind(&ParseResourceEntryAndRun, callback)));
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

  runner_->StartOperationWithRetry(
      new drive::InitiateUploadExistingFileOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          drive_file_path,
          content_type,
          content_length,
          resource_id,
          etag,
          callback));
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
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new drive::ResumeUploadOperation(
          operation_registry(),
          url_request_context_getter_,
          upload_mode,
          drive_file_path,
          upload_url,
          start_position,
          end_position,
          content_length,
          content_type,
          buf,
          base::Bind(&ParseResourceEntryForUploadRangeAndRun, callback),
          progress_callback));
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
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Unfortunately, there is no support of authorizing
  // third party application on Drive API v2.
  // As a temporary work around, we'll use the GData WAPI's api here.
  // TODO(hidehiko): Get rid of this hack, and use the Drive API when it is
  // supported.
  runner_->StartOperationWithRetry(
      new AuthorizeAppOperation(
          operation_registry(),
          url_request_context_getter_,
          wapi_url_generator_,
          callback,
          resource_id,
          app_id));
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

}  // namespace google_apis
