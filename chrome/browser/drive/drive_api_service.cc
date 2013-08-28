// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_requests.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_requests.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using google_apis::AppList;
using google_apis::AppListCallback;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::CancelCallback;
using google_apis::ChangeList;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileList;
using google_apis::FileResource;
using google_apis::GDATA_OTHER_ERROR;
using google_apis::GDATA_PARSE_ERROR;
using google_apis::GDataErrorCode;
using google_apis::AboutResourceCallback;
using google_apis::GetChangelistRequest;
using google_apis::GetContentCallback;
using google_apis::GetFilelistRequest;
using google_apis::GetResourceEntryCallback;
using google_apis::GetResourceEntryRequest;
using google_apis::GetResourceListCallback;
using google_apis::GetShareUrlCallback;
using google_apis::HTTP_NOT_IMPLEMENTED;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::Link;
using google_apis::ProgressCallback;
using google_apis::RequestSender;
using google_apis::ResourceEntry;
using google_apis::ResourceList;
using google_apis::UploadRangeCallback;
using google_apis::UploadRangeResponse;
using google_apis::drive::AboutGetRequest;
using google_apis::drive::AppsListRequest;
using google_apis::drive::ContinueGetFileListRequest;
using google_apis::drive::CopyResourceRequest;
using google_apis::drive::CreateDirectoryRequest;
using google_apis::drive::DeleteResourceRequest;
using google_apis::drive::DownloadFileRequest;
using google_apis::drive::FilesGetRequest;
using google_apis::drive::FilesPatchRequest;
using google_apis::drive::GetUploadStatusRequest;
using google_apis::drive::InitiateUploadExistingFileRequest;
using google_apis::drive::InitiateUploadNewFileRequest;
using google_apis::drive::InsertResourceRequest;
using google_apis::drive::ResumeUploadRequest;
using google_apis::drive::TrashResourceRequest;

namespace drive {

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
    scoped_refptr<base::TaskRunner> blocking_task_runner,
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
      blocking_task_runner.get(),
      FROM_HERE,
      base::Bind(&ParseResourceListOnBlockingPool, base::Passed(&value)),
      base::Bind(&DidParseResourceListOnBlockingPool, callback));
}

// Converts the FileResource value to ResourceEntry and runs |callback| on the
// UI thread.
void ConvertFileEntryToResourceEntryAndRun(
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

// Parses the FileResource value to ResourceEntry for upload range request,
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

void ExtractOpenUrlAndRun(const std::string& app_id,
                          const AuthorizeAppCallback& callback,
                          GDataErrorCode error,
                          scoped_ptr<FileResource> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, GURL());
    return;
  }

  const std::vector<FileResource::OpenWithLink>& open_with_links =
      value->open_with_links();
  for (size_t i = 0; i < open_with_links.size(); ++i) {
    if (open_with_links[i].app_id == app_id) {
      callback.Run(HTTP_SUCCESS, open_with_links[i].open_url);
      return;
    }
  }

  // Not found.
  callback.Run(GDATA_OTHER_ERROR, GURL());
}

// Ignores the |entry|, and runs the |callback|.
void EntryActionCallbackAdapter(
    const EntryActionCallback& callback,
    GDataErrorCode error, scoped_ptr<FileResource> entry) {
  callback.Run(error);
}

// The resource ID for the root directory for Drive API is defined in the spec:
// https://developers.google.com/drive/folder
const char kDriveApiRootDirectoryResourceId[] = "root";

}  // namespace

DriveAPIService::DriveAPIService(
    OAuth2TokenService* oauth2_token_service,
    net::URLRequestContextGetter* url_request_context_getter,
    base::TaskRunner* blocking_task_runner,
    const GURL& base_url,
    const GURL& base_download_url,
    const GURL& wapi_base_url,
    const std::string& custom_user_agent)
    : oauth2_token_service_(oauth2_token_service),
      url_request_context_getter_(url_request_context_getter),
      blocking_task_runner_(blocking_task_runner),
      url_generator_(base_url, base_download_url),
      wapi_url_generator_(wapi_base_url, base_download_url),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveAPIService::~DriveAPIService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (sender_.get())
    sender_->auth_service()->RemoveObserver(this);
}

void DriveAPIService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string> scopes;
  scopes.push_back(kDriveScope);
  scopes.push_back(kDriveAppsReadonlyScope);

  // GData WAPI token. These are for GetShareUrl().
  scopes.push_back(util::kDocsListScope);
  scopes.push_back(util::kDriveAppsScope);

  sender_.reset(new RequestSender(
     new google_apis::AuthService(
         oauth2_token_service_, url_request_context_getter_, scopes),
     url_request_context_getter_,
     blocking_task_runner_.get(),
     custom_user_agent_));
  sender_->auth_service()->AddObserver(this);
}

void DriveAPIService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveAPIService::CanSendRequest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

std::string DriveAPIService::CanonicalizeResourceId(
    const std::string& resource_id) const {
  return drive::util::CanonicalizeResourceId(resource_id);
}

std::string DriveAPIService::GetRootResourceId() const {
  return kDriveApiRootDirectoryResourceId;
}

CancelCallback DriveAPIService::GetAllResourceList(
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // The simplest way to fetch the all resources list looks files.list method,
  // but it seems impossible to know the returned list's changestamp.
  // Thus, instead, we use changes.list method with includeDeleted=false here.
  // The returned list should contain only resources currently existing.
  return sender_->StartRequestWithRetry(
      new GetChangelistRequest(
          sender_.get(),
          url_generator_,
          false,  // include deleted
          0,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  // Because children.list method on Drive API v2 returns only the list of
  // children's references, but we need all file resource list.
  // So, here we use files.list method instead, with setting parents query.
  // After the migration from GData WAPI to Drive API v2, we should clean the
  // code up by moving the responsibility to include "parents" in the query
  // to client side.
  // We aren't interested in files in trash in this context, neither.
  return sender_->StartRequestWithRetry(
      new GetFilelistRequest(
          sender_.get(),
          url_generator_,
          base::StringPrintf(
              "'%s' in parents and trashed = false",
              drive::util::EscapeQueryStringValue(
                  directory_resource_id).c_str()),
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::Search(
    const std::string& search_query,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetFilelistRequest(
          sender_.get(),
          url_generator_,
          drive::util::TranslateQuery(search_query),
          kMaxNumFilesResourcePerRequestForSearch,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::SearchByTitle(
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

  return sender_->StartRequestWithRetry(
      new GetFilelistRequest(
          sender_.get(),
          url_generator_,
          query,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::GetChangeList(
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new GetChangelistRequest(
          sender_.get(),
          url_generator_,
          true,  // include deleted
          start_changestamp,
          kMaxNumFilesResourcePerRequest,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::ContinueGetResourceList(
    const GURL& override_url,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new ContinueGetFileListRequest(
          sender_.get(),
          override_url,
          base::Bind(&ParseResourceListOnBlockingPoolAndRun,
                     blocking_task_runner_,
                     callback)));
}

CancelCallback DriveAPIService::GetRemainingChangeList(
    const std::string& page_token,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!page_token.empty());
  DCHECK(!callback.is_null());

  // Currently page_token is a URL.
  // TODO(hidehiko): Use actual page token.
  return ContinueGetResourceList(GURL(page_token), callback);
}

CancelCallback DriveAPIService::GetRemainingFileList(
    const std::string& page_token,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!page_token.empty());
  DCHECK(!callback.is_null());

  // Currently page_token is a URL.
  // TODO(hidehiko): Use actual page token.
  return ContinueGetResourceList(GURL(page_token), callback);
}

CancelCallback DriveAPIService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FilesGetRequest* request = new FilesGetRequest(
      sender_.get(), url_generator_,
      base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback));
  request->set_file_id(resource_id);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetShareUrl(
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Unfortunately "share url" is not yet supported on Drive API v2.
  // So, as a fallback, we use GData WAPI protocol for this method.
  // TODO(hidehiko): Get rid of this implementation when share url is
  // supported on Drive API v2.
  return sender_->StartRequestWithRetry(
      new GetResourceEntryRequest(sender_.get(),
                                  wapi_url_generator_,
                                  resource_id,
                                  embed_origin,
                                  base::Bind(&util::ParseShareUrlAndRun,
                                             callback)));
}

CancelCallback DriveAPIService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new AboutGetRequest(sender_.get(), url_generator_, callback));
}

CancelCallback DriveAPIService::GetAppList(const AppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new AppsListRequest(sender_.get(), url_generator_, callback));
}

CancelCallback DriveAPIService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  return sender_->StartRequestWithRetry(
      new DownloadFileRequest(sender_.get(),
                              url_generator_,
                              resource_id,
                              local_cache_path,
                              download_action_callback,
                              get_content_callback,
                              progress_callback));
}

CancelCallback DriveAPIService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(new TrashResourceRequest(
      sender_.get(),
      url_generator_,
      resource_id,
      callback));
}

CancelCallback DriveAPIService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new CreateDirectoryRequest(
          sender_.get(),
          url_generator_,
          parent_resource_id,
          directory_title,
          base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback)));
}

CancelCallback DriveAPIService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new CopyResourceRequest(
          sender_.get(),
          url_generator_,
          resource_id,
          parent_resource_id,
          new_title,
          base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback)));
}

CancelCallback DriveAPIService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new CopyResourceRequest(
          sender_.get(),
          url_generator_,
          resource_id,
          std::string(),  // parent_resource_id.
          new_title,
          base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback)));
}

CancelCallback DriveAPIService::MoveResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FilesPatchRequest* request = new FilesPatchRequest(
      sender_.get(), url_generator_,
      base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback));
  request->set_file_id(resource_id);
  request->set_title(new_title);
  if (!parent_resource_id.empty())
    request->add_parent(parent_resource_id);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FilesPatchRequest* request = new FilesPatchRequest(
      sender_.get(), url_generator_,
      base::Bind(&EntryActionCallbackAdapter, callback));
  request->set_file_id(resource_id);
  request->set_title(new_title);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::TouchResource(
    const std::string& resource_id,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!modified_date.is_null());
  DCHECK(!last_viewed_by_me_date.is_null());
  DCHECK(!callback.is_null());

  FilesPatchRequest* request = new FilesPatchRequest(
      sender_.get(), url_generator_,
      base::Bind(&ConvertFileEntryToResourceEntryAndRun, callback));
  // Need to set setModifiedDate to true to overwrite modifiedDate.
  request->set_set_modified_date(true);

  // Need to set updateViewedDate to false, otherwise the lastViewedByMeDate
  // will be set to the request time (not the specified time via request).
  request->set_update_viewed_date(false);

  request->set_modified_date(modified_date);
  request->set_last_viewed_by_me_date(last_viewed_by_me_date);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new InsertResourceRequest(
          sender_.get(),
          url_generator_,
          parent_resource_id,
          resource_id,
          callback));
}

CancelCallback DriveAPIService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new DeleteResourceRequest(
          sender_.get(),
          url_generator_,
          parent_resource_id,
          resource_id,
          callback));
}

CancelCallback DriveAPIService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new InitiateUploadNewFileRequest(
          sender_.get(),
          url_generator_,
          content_type,
          content_length,
          parent_resource_id,
          title,
          callback));
}

CancelCallback DriveAPIService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new InitiateUploadExistingFileRequest(
          sender_.get(),
          url_generator_,
          content_type,
          content_length,
          resource_id,
          etag,
          callback));
}

CancelCallback DriveAPIService::ResumeUpload(
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
      new ResumeUploadRequest(
          sender_.get(),
          upload_url,
          start_position,
          end_position,
          content_length,
          content_type,
          local_file_path,
          base::Bind(&ParseResourceEntryForUploadRangeAndRun, callback),
          progress_callback));
}

CancelCallback DriveAPIService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(new GetUploadStatusRequest(
      sender_.get(),
      upload_url,
      content_length,
      base::Bind(&ParseResourceEntryForUploadRangeAndRun, callback)));
}

CancelCallback DriveAPIService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FilesGetRequest* request = new FilesGetRequest(
      sender_.get(), url_generator_,
      base::Bind(&ExtractOpenUrlAndRun, app_id, callback));
  request->set_file_id(resource_id);
  return sender_->StartRequestWithRetry(request);
}

bool DriveAPIService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sender_->auth_service()->HasAccessToken();
}

void DriveAPIService::RequestAccessToken(const AuthStatusCallback& callback) {
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

bool DriveAPIService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sender_->auth_service()->HasRefreshToken();
}

void DriveAPIService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sender_->auth_service()->ClearAccessToken();
}

void DriveAPIService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sender_->auth_service()->ClearRefreshToken();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
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
