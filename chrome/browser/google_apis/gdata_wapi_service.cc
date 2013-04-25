// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"

using content::BrowserThread;

namespace google_apis {

namespace {

// OAuth2 scopes for the documents API.
const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";

// The resource ID for the root directory for WAPI is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
const char kWapiRootDirectoryResourceId[] = "folder:root";

// Parses the JSON value to ResourceEntry runs |callback|.
void ParseResourceEntryAndRun(const GetResourceEntryCallback& callback,
                              GDataErrorCode error,
                              scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  // Parsing ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> entry =
      google_apis::ResourceEntry::ExtractAndParse(*value);
  if (!entry) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(error, entry.Pass());
}

void ParseAboutResourceAndRun(
    const GetAboutResourceCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<AboutResource> about_resource;
  if (account_metadata) {
    about_resource = AboutResource::CreateFromAccountMetadata(
        *account_metadata, kWapiRootDirectoryResourceId);
  }

  callback.Run(error, about_resource.Pass());
}

void ParseAppListAndRun(
    const GetAppListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<AppList> app_list;
  if (account_metadata) {
    app_list = AppList::CreateFromAccountMetadata(*account_metadata);
  }

  callback.Run(error, app_list.Pass());
}

}  // namespace

GDataWapiService::GDataWapiService(
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& base_url,
    const std::string& custom_user_agent)
    : url_request_context_getter_(url_request_context_getter),
      runner_(NULL),
      url_generator_(base_url),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataWapiService::~GDataWapiService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get())
    runner_->auth_service()->RemoveObserver(this);
}

AuthService* GDataWapiService::auth_service_for_testing() {
  return runner_->auth_service();
}

void GDataWapiService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string> scopes;
  scopes.push_back(kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  // Drive App scope is required for even WAPI v3 apps access.
  scopes.push_back(kDriveAppsScope);
  runner_.reset(new OperationRunner(profile,
                                    url_request_context_getter_,
                                    scopes,
                                    custom_user_agent_));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
}

void GDataWapiService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void GDataWapiService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool GDataWapiService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

void GDataWapiService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

bool GDataWapiService::CancelForFilePath(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->CancelForFilePath(file_path);
}

std::string GDataWapiService::GetRootResourceId() const {
  return kWapiRootDirectoryResourceId;
}

// Because GData WAPI support is expected to be gone somehow soon by migration
// to the Drive API v2, so we'll reuse GetResourceListOperation to implement
// following methods, instead of cleaning the operation class.

void GDataWapiService::GetAllResourceList(
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceListOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   GURL(),         // No override url
                                   0,              // start changestamp
                                   std::string(),  // empty search query
                                   std::string(),  // no directory resource id
                                   callback));
}

void GDataWapiService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceListOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   GURL(),         // No override url
                                   0,              // start changestamp
                                   std::string(),  // empty search query
                                   directory_resource_id,
                                   callback));
}

void GDataWapiService::Search(const std::string& search_query,
                              const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceListOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   GURL(),         // No override url
                                   0,              // start changestamp
                                   search_query,
                                   std::string(),  // no directory resource id
                                   callback));
}

void GDataWapiService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new SearchByTitleOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          title,
          directory_resource_id,
          callback));
}

void GDataWapiService::GetChangeList(int64 start_changestamp,
                                     const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceListOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   GURL(),         // No override url
                                   start_changestamp,
                                   std::string(),  // empty search query
                                   std::string(),  // no directory resource id
                                   callback));
}

void GDataWapiService::ContinueGetResourceList(
    const GURL& override_url,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!override_url.is_empty());
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceListOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   override_url,
                                   0,              // start changestamp
                                   std::string(),  // empty search query
                                   std::string(),  // no directory resource id
                                   callback));
}

void GDataWapiService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceEntryOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          resource_id,
          base::Bind(&ParseResourceEntryAndRun, callback)));
}

void GDataWapiService::GetAccountMetadata(
    const GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          callback,
          true));  // Include installed apps.
}

void GDataWapiService::GetAboutResource(
    const GetAboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAboutResourceAndRun, callback),
          false));  // Exclude installed apps.
}

void GDataWapiService::GetAppList(const GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAppListAndRun, callback),
          true));  // Include installed apps.
}

void GDataWapiService::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback and progress_callback may be null.

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

void GDataWapiService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new DeleteResourceOperation(operation_registry(),
                                  url_request_context_getter_,
                                  url_generator_,
                                  callback,
                                  resource_id,
                                  etag));
}

void GDataWapiService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new CreateDirectoryOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   base::Bind(&ParseResourceEntryAndRun,
                                              callback),
                                   parent_resource_id,
                                   directory_name));
}

void GDataWapiService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new CopyHostedDocumentOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseResourceEntryAndRun, callback),
          resource_id,
          new_name));
}

void GDataWapiService::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new RenameResourceOperation(operation_registry(),
                                  url_request_context_getter_,
                                  url_generator_,
                                  callback,
                                  resource_id,
                                  new_name));
}

void GDataWapiService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new AddResourceToDirectoryOperation(operation_registry(),
                                          url_request_context_getter_,
                                          url_generator_,
                                          callback,
                                          parent_resource_id,
                                          resource_id));
}

void GDataWapiService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new RemoveResourceFromDirectoryOperation(operation_registry(),
                                               url_request_context_getter_,
                                               url_generator_,
                                               callback,
                                               parent_resource_id,
                                               resource_id));
}

void GDataWapiService::InitiateUploadNewFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!parent_resource_id.empty());

  runner_->StartOperationWithRetry(
      new InitiateUploadNewFileOperation(operation_registry(),
                                         url_request_context_getter_,
                                         url_generator_,
                                         callback,
                                         drive_file_path,
                                         content_type,
                                         content_length,
                                         parent_resource_id,
                                         title));
}

void GDataWapiService::InitiateUploadExistingFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!resource_id.empty());

  runner_->StartOperationWithRetry(
      new InitiateUploadExistingFileOperation(operation_registry(),
                                              url_request_context_getter_,
                                              url_generator_,
                                              callback,
                                              drive_file_path,
                                              content_type,
                                              content_length,
                                              resource_id,
                                              etag));
}

void GDataWapiService::ResumeUpload(
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
      new ResumeUploadOperation(operation_registry(),
                                url_request_context_getter_,
                                callback,
                                progress_callback,
                                upload_mode,
                                drive_file_path,
                                upload_url,
                                start_position,
                                end_position,
                                content_length,
                                content_type,
                                buf));
}

void GDataWapiService::GetUploadStatus(
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetUploadStatusOperation(operation_registry(),
                                   url_request_context_getter_,
                                   callback,
                                   upload_mode,
                                   drive_file_path,
                                   upload_url,
                                   content_length));
}

void GDataWapiService::AuthorizeApp(const std::string& resource_id,
                                    const std::string& app_id,
                                    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new AuthorizeAppOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          callback,
          resource_id,
          app_id));
}

bool GDataWapiService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasAccessToken();
}

bool GDataWapiService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasRefreshToken();
}

void GDataWapiService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearAccessToken();
}

void GDataWapiService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearRefreshToken();
}

OperationRegistry* GDataWapiService::operation_registry() const {
  return runner_->operation_registry();
}

void GDataWapiService::OnOAuth2RefreshTokenChanged() {
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
