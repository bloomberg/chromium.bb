// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/gdata_wapi_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_requests.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using google_apis::AboutResource;
using google_apis::AboutResourceCallback;
using google_apis::AccountMetadata;
using google_apis::AddResourceToDirectoryRequest;
using google_apis::AppList;
using google_apis::AppListCallback;
using google_apis::AuthService;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::AuthorizeAppRequest;
using google_apis::CancelCallback;
using google_apis::CopyHostedDocumentRequest;
using google_apis::CreateDirectoryRequest;
using google_apis::DeleteResourceRequest;
using google_apis::DownloadActionCallback;
using google_apis::DownloadFileRequest;
using google_apis::EntryActionCallback;
using google_apis::GDATA_PARSE_ERROR;
using google_apis::GDataErrorCode;
using google_apis::GetAccountMetadataRequest;
using google_apis::GetContentCallback;
using google_apis::GetResourceEntryCallback;
using google_apis::GetResourceEntryRequest;
using google_apis::GetResourceListCallback;
using google_apis::GetResourceListRequest;
using google_apis::GetShareUrlCallback;
using google_apis::GetUploadStatusRequest;
using google_apis::HTTP_NOT_IMPLEMENTED;
using google_apis::InitiateUploadCallback;
using google_apis::InitiateUploadExistingFileRequest;
using google_apis::InitiateUploadNewFileRequest;
using google_apis::Link;
using google_apis::ProgressCallback;
using google_apis::RemoveResourceFromDirectoryRequest;
using google_apis::RenameResourceRequest;
using google_apis::RequestSender;
using google_apis::ResourceEntry;
using google_apis::ResumeUploadRequest;
using google_apis::SearchByTitleRequest;
using google_apis::UploadRangeCallback;

namespace drive {

namespace {

// OAuth2 scopes for the documents API.
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";

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
    const AboutResourceCallback& callback,
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
    const AppListCallback& callback,
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
    OAuth2TokenService* oauth2_token_service,
    net::URLRequestContextGetter* url_request_context_getter,
    base::TaskRunner* blocking_task_runner,
    const GURL& base_url,
    const GURL& base_download_url,
    const std::string& custom_user_agent)
    : oauth2_token_service_(oauth2_token_service),
      url_request_context_getter_(url_request_context_getter),
      blocking_task_runner_(blocking_task_runner),
      url_generator_(base_url, base_download_url),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataWapiService::~GDataWapiService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (sender_.get())
    sender_->auth_service()->RemoveObserver(this);
}

void GDataWapiService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string> scopes;
  scopes.push_back(util::kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  // Drive App scope is required for even WAPI v3 apps access.
  scopes.push_back(util::kDriveAppsScope);
  sender_.reset(new RequestSender(
      new AuthService(
          oauth2_token_service_, url_request_context_getter_, scopes),
      url_request_context_getter_,
      blocking_task_runner_.get(),
      custom_user_agent_));

  sender_->auth_service()->AddObserver(this);
}

void GDataWapiService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void GDataWapiService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool GDataWapiService::CanSendRequest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

std::string GDataWapiService::CanonicalizeResourceId(
    const std::string& resource_id) const {
  return resource_id;
}

std::string GDataWapiService::GetRootResourceId() const {
  return kWapiRootDirectoryResourceId;
}

// Because GData WAPI support is expected to be gone somehow soon by migration
// to the Drive API v2, so we'll reuse GetResourceListRequest to implement
// following methods, instead of cleaning the request class.

CancelCallback GDataWapiService::GetAllResourceList(
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceListRequest(sender_.get(),
                                 url_generator_,
                                 GURL(),         // No override url
                                 0,              // start changestamp
                                 std::string(),  // empty search query
                                 std::string(),  // no directory resource id
                                 callback));
}

CancelCallback GDataWapiService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceListRequest(sender_.get(),
                                 url_generator_,
                                 GURL(),         // No override url
                                 0,              // start changestamp
                                 std::string(),  // empty search query
                                 directory_resource_id,
                                 callback));
}

CancelCallback GDataWapiService::Search(
    const std::string& search_query,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceListRequest(sender_.get(),
                                 url_generator_,
                                 GURL(),         // No override url
                                 0,              // start changestamp
                                 search_query,
                                 std::string(),  // no directory resource id
                                 callback));
}

CancelCallback GDataWapiService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new SearchByTitleRequest(sender_.get(),
                               url_generator_,
                               title,
                               directory_resource_id,
                               callback));
}

CancelCallback GDataWapiService::GetChangeList(
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceListRequest(sender_.get(),
                                 url_generator_,
                                 GURL(),         // No override url
                                 start_changestamp,
                                 std::string(),  // empty search query
                                 std::string(),  // no directory resource id
                                 callback));
}

CancelCallback GDataWapiService::ContinueGetResourceList(
    const GURL& override_url,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!override_url.is_empty());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceListRequest(sender_.get(),
                                 url_generator_,
                                 override_url,
                                 0,              // start changestamp
                                 std::string(),  // empty search query
                                 std::string(),  // no directory resource id
                                 callback));
}

CancelCallback GDataWapiService::GetRemainingChangeList(
    const std::string& page_token,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!page_token.empty());
  DCHECK(!callback.is_null());

  // The page token for the GData WAPI is an URL.
  return ContinueGetResourceList(GURL(page_token), callback);
}

CancelCallback GDataWapiService::GetRemainingFileList(
    const std::string& page_token,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!page_token.empty());
  DCHECK(!callback.is_null());

  // The page token for the GData WAPI is an URL.
  return ContinueGetResourceList(GURL(page_token), callback);
}

CancelCallback GDataWapiService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceEntryRequest(sender_.get(),
                                  url_generator_,
                                  resource_id,
                                  GURL(),
                                  base::Bind(&ParseResourceEntryAndRun,
                                             callback)));
}

CancelCallback GDataWapiService::GetShareUrl(
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetResourceEntryRequest(sender_.get(),
                                  url_generator_,
                                  resource_id,
                                  embed_origin,
                                  base::Bind(&util::ParseShareUrlAndRun,
                                             callback)));
}

CancelCallback GDataWapiService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetAccountMetadataRequest(
          sender_.get(),
          url_generator_,
          base::Bind(&ParseAboutResourceAndRun, callback),
          false));  // Exclude installed apps.
}

CancelCallback GDataWapiService::GetAppList(const AppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetAccountMetadataRequest(sender_.get(),
                                    url_generator_,
                                    base::Bind(&ParseAppListAndRun, callback),
                                    true));  // Include installed apps.
}

CancelCallback GDataWapiService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback and progress_callback may be null.

  return sender_->StartRequestWithRetry(
      new DownloadFileRequest(sender_.get(),
                              url_generator_,
                              download_action_callback,
                              get_content_callback,
                              progress_callback,
                              resource_id,
                              local_cache_path));
}

CancelCallback GDataWapiService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new DeleteResourceRequest(sender_.get(),
                                url_generator_,
                                callback,
                                resource_id,
                                etag));
}

CancelCallback GDataWapiService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new CreateDirectoryRequest(sender_.get(),
                                 url_generator_,
                                 base::Bind(&ParseResourceEntryAndRun,
                                            callback),
                                 parent_resource_id,
                                 directory_title));
}

CancelCallback GDataWapiService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // GData WAPI doesn't support "copy" of regular files.
  // This method should never be called if GData WAPI is enabled.
  // Instead, client code should download the file (if needed) and upload it.
  NOTREACHED();
  return CancelCallback();
}

CancelCallback GDataWapiService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new CopyHostedDocumentRequest(sender_.get(),
                                    url_generator_,
                                    base::Bind(&ParseResourceEntryAndRun,
                                               callback),
                                    resource_id,
                                    new_title));
}

CancelCallback GDataWapiService::MoveResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // GData WAPI doesn't support to "move" resources.
  // This method should never be called if GData WAPI is enabled.
  // Instead, client code should rename the file, add new parent, and then
  // remove the old parent.
  NOTREACHED();
  return CancelCallback();
}

CancelCallback GDataWapiService::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new RenameResourceRequest(sender_.get(),
                                url_generator_,
                                callback,
                                resource_id,
                                new_title));
}

CancelCallback GDataWapiService::TouchResource(
    const std::string& resource_id,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!modified_date.is_null());
  DCHECK(!last_viewed_by_me_date.is_null());
  DCHECK(!callback.is_null());

  // Unfortunately, there is no way to support this method on GData WAPI.
  // So, this should always return an error.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_IMPLEMENTED,
                 base::Passed(scoped_ptr<ResourceEntry>())));
  return base::Bind(&base::DoNothing);
}

CancelCallback GDataWapiService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new AddResourceToDirectoryRequest(sender_.get(),
                                        url_generator_,
                                        callback,
                                        parent_resource_id,
                                        resource_id));
}

CancelCallback GDataWapiService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new RemoveResourceFromDirectoryRequest(sender_.get(),
                                             url_generator_,
                                             callback,
                                             parent_resource_id,
                                             resource_id));
}

CancelCallback GDataWapiService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!parent_resource_id.empty());

  return sender_->StartRequestWithRetry(
      new InitiateUploadNewFileRequest(sender_.get(),
                                       url_generator_,
                                       callback,
                                       content_type,
                                       content_length,
                                       parent_resource_id,
                                       title));
}

CancelCallback GDataWapiService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!resource_id.empty());

  return sender_->StartRequestWithRetry(
      new InitiateUploadExistingFileRequest(sender_.get(),
                                            url_generator_,
                                            callback,
                                            content_type,
                                            content_length,
                                            resource_id,
                                            etag));
}

CancelCallback GDataWapiService::ResumeUpload(
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new ResumeUploadRequest(sender_.get(),
                              callback,
                              progress_callback,
                              upload_url,
                              start_position,
                              end_position,
                              content_length,
                              content_type,
                              local_file_path));
}

CancelCallback GDataWapiService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetUploadStatusRequest(sender_.get(),
                                 callback,
                                 upload_url,
                                 content_length));
}

CancelCallback GDataWapiService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new AuthorizeAppRequest(sender_.get(),
                              url_generator_,
                              callback,
                              resource_id,
                              app_id));
}

bool GDataWapiService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return sender_->auth_service()->HasAccessToken();
}

void GDataWapiService::RequestAccessToken(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  const std::string access_token = sender_->auth_service()->access_token();
  if (!access_token.empty()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, access_token);
    return;
  }

  // Retrieve the new auth token.
  sender_->auth_service()->StartAuthentication(callback);
}

bool GDataWapiService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sender_->auth_service()->HasRefreshToken();
}

void GDataWapiService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sender_->auth_service()->ClearAccessToken();
}

void GDataWapiService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sender_->auth_service()->ClearRefreshToken();
}

void GDataWapiService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanSendRequest()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToSendRequests());
  } else if (!HasRefreshToken()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnRefreshTokenInvalid());
  }
}

}  // namespace drive
