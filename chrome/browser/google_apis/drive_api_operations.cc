// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_operations.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/operation_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {
namespace {

const char kContentTypeApplicationJson[] = "application/json";
const char kDirectoryMimeType[] = "application/vnd.google-apps.folder";
const char kParentLinkKind[] = "drive#fileLink";

// Parses the JSON value to a resource typed |T| and runs |callback| on the UI
// thread once parsing is done.
template<typename T>
void ParseJsonAndRun(
    const base::Callback<void(GDataErrorCode, scoped_ptr<T>)>& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<T> resource;
  if (value) {
    resource = T::CreateFrom(*value);
    if (!resource) {
      // Failed to parse the JSON value, although the JSON value is available,
      // so let the callback know the parsing error.
      error = GDATA_PARSE_ERROR;
    }
  }

  callback.Run(error, resource.Pass());
}

// Parses the JSON value to FileResource instance and runs |callback| on the
// UI thread once parsing is done.
// This is customized version of ParseJsonAndRun defined above to adapt the
// remaining response type.
void ParseFileResourceWithUploadRangeAndRun(
    const drive::UploadRangeCallback& callback,
    const UploadRangeResponse& response,
    scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<FileResource> file_resource;
  if (value) {
    file_resource = FileResource::CreateFrom(*value);
    if (!file_resource) {
      callback.Run(
          UploadRangeResponse(GDATA_PARSE_ERROR,
                              response.start_position_received,
                              response.end_position_received),
          scoped_ptr<FileResource>());
      return;
    }
  }

  callback.Run(response, file_resource.Pass());
}

}  // namespace

//============================== GetAboutOperation =============================

GetAboutOperation::GetAboutOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const GetAboutResourceCallback& callback)
    : GetDataOperation(registry, url_request_context_getter,
                       base::Bind(&ParseJsonAndRun<AboutResource>, callback)),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

GetAboutOperation::~GetAboutOperation() {}

GURL GetAboutOperation::GetURL() const {
  return url_generator_.GetAboutUrl();
}

//============================== GetApplistOperation ===========================

GetApplistOperation::GetApplistOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

GetApplistOperation::~GetApplistOperation() {}

GURL GetApplistOperation::GetURL() const {
  return url_generator_.GetApplistUrl();
}

//============================ GetChangelistOperation ==========================

GetChangelistOperation::GetChangelistOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    bool include_deleted,
    int64 start_changestamp,
    int max_results,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      include_deleted_(include_deleted),
      start_changestamp_(start_changestamp),
      max_results_(max_results) {
  DCHECK(!callback.is_null());
}

GetChangelistOperation::~GetChangelistOperation() {}

GURL GetChangelistOperation::GetURL() const {
  return url_generator_.GetChangelistUrl(
      include_deleted_, start_changestamp_, max_results_);
}

//============================= GetFilelistOperation ===========================

GetFilelistOperation::GetFilelistOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& search_string,
    int max_results,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      search_string_(search_string),
      max_results_(max_results) {
  DCHECK(!callback.is_null());
}

GetFilelistOperation::~GetFilelistOperation() {}

GURL GetFilelistOperation::GetURL() const {
  return url_generator_.GetFilelistUrl(search_string_, max_results_);
}

//=============================== GetFileOperation =============================

GetFileOperation::GetFileOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& file_id,
    const FileResourceCallback& callback)
    : GetDataOperation(registry, url_request_context_getter,
                       base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      file_id_(file_id) {
  DCHECK(!callback.is_null());
}

GetFileOperation::~GetFileOperation() {}

GURL GetFileOperation::GetURL() const {
  return url_generator_.GetFileUrl(file_id_);
}

namespace drive {

//======================= ContinueGetFileListOperation =========================

ContinueGetFileListOperation::ContinueGetFileListOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& url,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_(url) {
  DCHECK(!callback.is_null());
}

ContinueGetFileListOperation::~ContinueGetFileListOperation() {}

GURL ContinueGetFileListOperation::GetURL() const {
  return url_;
}

//========================== CreateDirectoryOperation ==========================

CreateDirectoryOperation::CreateDirectoryOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const FileResourceCallback& callback)
    : GetDataOperation(registry, url_request_context_getter,
                       base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      directory_name_(directory_name) {
  DCHECK(!callback.is_null());
  DCHECK(!parent_resource_id_.empty());
  DCHECK(!directory_name_.empty());
}

CreateDirectoryOperation::~CreateDirectoryOperation() {}

GURL CreateDirectoryOperation::GetURL() const {
  return url_generator_.GetFilesUrl();
}

net::URLFetcher::RequestType CreateDirectoryOperation::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool CreateDirectoryOperation::GetContentData(std::string* upload_content_type,
                                              std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", directory_name_);
  {
    base::DictionaryValue* parent_value = new base::DictionaryValue;
    parent_value->SetString("id", parent_resource_id_);
    base::ListValue* parent_list_value = new base::ListValue;
    parent_list_value->Append(parent_value);
    root.Set("parents", parent_list_value);
  }
  root.SetString("mimeType", kDirectoryMimeType);

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "CreateDirectory data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== RenameResourceOperation ==========================

RenameResourceOperation::RenameResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      new_name_(new_name) {
  DCHECK(!callback.is_null());
}

RenameResourceOperation::~RenameResourceOperation() {}

net::URLFetcher::RequestType RenameResourceOperation::GetRequestType() const {
  return net::URLFetcher::PATCH;
}

std::vector<std::string>
RenameResourceOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL RenameResourceOperation::GetURL() const {
  return url_generator_.GetFileUrl(resource_id_);
}

bool RenameResourceOperation::GetContentData(std::string* upload_content_type,
                                             std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", new_name_);
  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "RenameResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== CopyResourceOperation ============================

CopyResourceOperation::CopyResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const std::string& new_name,
    const FileResourceCallback& callback)
    : GetDataOperation(registry, url_request_context_getter,
                       base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      resource_id_(resource_id),
      new_name_(new_name) {
  DCHECK(!callback.is_null());
}

CopyResourceOperation::~CopyResourceOperation() {
}

net::URLFetcher::RequestType CopyResourceOperation::GetRequestType() const {
  return net::URLFetcher::POST;
}

GURL CopyResourceOperation::GetURL() const {
  return url_generator_.GetFileCopyUrl(resource_id_);
}

bool CopyResourceOperation::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", new_name_);
  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "CopyResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== TrashResourceOperation ===========================

TrashResourceOperation::TrashResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

TrashResourceOperation::~TrashResourceOperation() {}

GURL TrashResourceOperation::GetURL() const {
  return url_generator_.GetFileTrashUrl(resource_id_);
}

net::URLFetcher::RequestType TrashResourceOperation::GetRequestType() const {
  return net::URLFetcher::POST;
}

//========================== InsertResourceOperation ===========================

InsertResourceOperation::InsertResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

InsertResourceOperation::~InsertResourceOperation() {}

GURL InsertResourceOperation::GetURL() const {
  return url_generator_.GetChildrenUrl(parent_resource_id_);
}

net::URLFetcher::RequestType InsertResourceOperation::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool InsertResourceOperation::GetContentData(std::string* upload_content_type,
                                             std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("id", resource_id_);
  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "InsertResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//========================== DeleteResourceOperation ===========================

DeleteResourceOperation::DeleteResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

DeleteResourceOperation::~DeleteResourceOperation() {}

GURL DeleteResourceOperation::GetURL() const {
  return url_generator_.GetChildrenUrlForRemoval(
      parent_resource_id_, resource_id_);
}

net::URLFetcher::RequestType DeleteResourceOperation::GetRequestType() const {
  return net::URLFetcher::DELETE_REQUEST;
}

//======================= InitiateUploadNewFileOperation =======================

InitiateUploadNewFileOperation::InitiateUploadNewFileOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback)
    : InitiateUploadOperationBase(registry,
                                  url_request_context_getter,
                                  callback,
                                  drive_file_path,
                                  content_type,
                                  content_length),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      title_(title) {
}

InitiateUploadNewFileOperation::~InitiateUploadNewFileOperation() {}

GURL InitiateUploadNewFileOperation::GetURL() const {
  return url_generator_.GetInitiateUploadNewFileUrl();
}

net::URLFetcher::RequestType
InitiateUploadNewFileOperation::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool InitiateUploadNewFileOperation::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", title_);

  // Fill parent link.
  {
    scoped_ptr<base::DictionaryValue> parent(new base::DictionaryValue);
    parent->SetString("kind", kParentLinkKind);
    parent->SetString("id", parent_resource_id_);

    scoped_ptr<base::ListValue> parents(new base::ListValue);
    parents->Append(parent.release());

    root.Set("parents", parents.release());
  }

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "InitiateUploadNewFile data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//===================== InitiateUploadExistingFileOperation ====================

InitiateUploadExistingFileOperation::InitiateUploadExistingFileOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DriveApiUrlGenerator& url_generator,
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback)
    : InitiateUploadOperationBase(registry,
                                  url_request_context_getter,
                                  callback,
                                  drive_file_path,
                                  content_type,
                                  content_length),
      url_generator_(url_generator),
      resource_id_(resource_id),
      etag_(etag) {
}

InitiateUploadExistingFileOperation::~InitiateUploadExistingFileOperation() {}

GURL InitiateUploadExistingFileOperation::GetURL() const {
  return url_generator_.GetInitiateUploadExistingFileUrl(resource_id_);
}

net::URLFetcher::RequestType
InitiateUploadExistingFileOperation::GetRequestType() const {
  return net::URLFetcher::PUT;
}

std::vector<std::string>
InitiateUploadExistingFileOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      InitiateUploadOperationBase::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

//============================ ResumeUploadOperation ===========================

ResumeUploadOperation::ResumeUploadOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_location,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const scoped_refptr<net::IOBuffer>& buf,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback)
    : ResumeUploadOperationBase(registry,
                                url_request_context_getter,
                                upload_mode,
                                drive_file_path,
                                upload_location,
                                start_position,
                                end_position,
                                content_length,
                                content_type,
                                buf),
      callback_(callback),
      progress_callback_(progress_callback) {
  DCHECK(!callback_.is_null());
}

ResumeUploadOperation::~ResumeUploadOperation() {}

void ResumeUploadOperation::OnRangeOperationComplete(
    const UploadRangeResponse& response, scoped_ptr<base::Value> value) {
  ParseFileResourceWithUploadRangeAndRun(callback_, response, value.Pass());
}

void ResumeUploadOperation::OnURLFetchUploadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

}  // namespace drive
}  // namespace google_apis
