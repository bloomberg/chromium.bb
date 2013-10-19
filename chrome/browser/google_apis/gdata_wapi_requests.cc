// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_requests.h"

#include "base/location.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/request_util.h"
#include "third_party/libxml/chromium/libxml_utils.h"

using net::URLFetcher;

namespace google_apis {

namespace {

// Parses the JSON value to ResourceList.
scoped_ptr<ResourceList> ParseResourceListOnBlockingPool(
    scoped_ptr<base::Value> value) {
  DCHECK(value);

  return ResourceList::ExtractAndParse(*value);
}

// Runs |callback| with |error| and |resource_list|, but replace the error code
// with GDATA_PARSE_ERROR, if there was a parsing error.
void DidParseResourceListOnBlockingPool(
    const GetResourceListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<ResourceList> resource_list) {
  DCHECK(!callback.is_null());

  // resource_list being NULL indicates there was a parsing error.
  if (!resource_list)
    error = GDATA_PARSE_ERROR;

  callback.Run(error, resource_list.Pass());
}

// Parses the JSON value to ResourceList on the blocking pool and runs
// |callback| on the UI thread once parsing is done.
void ParseResourceListAndRun(
    scoped_refptr<base::TaskRunner> blocking_task_runner,
    const GetResourceListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceList>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner,
      FROM_HERE,
      base::Bind(&ParseResourceListOnBlockingPool, base::Passed(&value)),
      base::Bind(&DidParseResourceListOnBlockingPool, callback, error));
}

// Parses the JSON value to AccountMetadata and runs |callback| on the UI
// thread once parsing is done.
void ParseAccounetMetadataAndRun(const GetAccountMetadataCallback& callback,
                                 GDataErrorCode error,
                                 scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<AccountMetadata>());
    return;
  }

  // Parsing AccountMetadata is cheap enough to do on UI thread.
  scoped_ptr<AccountMetadata> entry =
      google_apis::AccountMetadata::CreateFrom(*value);
  if (!entry) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<AccountMetadata>());
    return;
  }

  callback.Run(error, entry.Pass());
}

// Parses the |value| to ResourceEntry with error handling.
// This is designed to be used for ResumeUploadRequest and
// GetUploadStatusRequest.
scoped_ptr<ResourceEntry> ParseResourceEntry(scoped_ptr<base::Value> value) {
  scoped_ptr<ResourceEntry> entry;
  if (value.get()) {
    entry = ResourceEntry::ExtractAndParse(*value);

    // Note: |value| may be NULL, in particular if the callback is for a
    // failure.
    if (!entry.get())
      LOG(WARNING) << "Invalid entry received on upload.";
  }

  return entry.Pass();
}

// Extracts the open link url from the JSON Feed. Used by AuthorizeApp().
void ParseOpenLinkAndRun(const std::string& app_id,
                         const AuthorizeAppCallback& callback,
                         GDataErrorCode error,
                         scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, GURL());
    return;
  }

  // Parsing ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> resource_entry = ParseResourceEntry(value.Pass());
  if (!resource_entry) {
    callback.Run(GDATA_PARSE_ERROR, GURL());
    return;
  }

  // Look for the link to open the file with the app with |app_id|.
  const ScopedVector<Link>& resource_links = resource_entry->links();
  GURL open_link;
  for (size_t i = 0; i < resource_links.size(); ++i) {
    const Link& link = *resource_links[i];
    if (link.type() == Link::LINK_OPEN_WITH && link.app_id() == app_id) {
      open_link = link.href();
      break;
    }
  }

  if (open_link.is_empty())
    error = GDATA_OTHER_ERROR;

  callback.Run(error, open_link);
}

}  // namespace

//============================ GetResourceListRequest ========================

GetResourceListRequest::GetResourceListRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const GURL& override_url,
    int64 start_changestamp,
    const std::string& search_string,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback)
    : GetDataRequest(
          sender,
          base::Bind(&ParseResourceListAndRun,
                     make_scoped_refptr(sender->blocking_task_runner()),
                     callback)),
      url_generator_(url_generator),
      override_url_(override_url),
      start_changestamp_(start_changestamp),
      search_string_(search_string),
      directory_resource_id_(directory_resource_id) {
  DCHECK(!callback.is_null());
}

GetResourceListRequest::~GetResourceListRequest() {}

GURL GetResourceListRequest::GetURL() const {
  return url_generator_.GenerateResourceListUrl(override_url_,
                                                start_changestamp_,
                                                search_string_,
                                                directory_resource_id_);
}

//============================ SearchByTitleRequest ==========================

SearchByTitleRequest::SearchByTitleRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback)
    : GetDataRequest(
          sender,
          base::Bind(&ParseResourceListAndRun,
                     make_scoped_refptr(sender->blocking_task_runner()),
                     callback)),
      url_generator_(url_generator),
      title_(title),
      directory_resource_id_(directory_resource_id) {
  DCHECK(!callback.is_null());
}

SearchByTitleRequest::~SearchByTitleRequest() {}

GURL SearchByTitleRequest::GetURL() const {
  return url_generator_.GenerateSearchByTitleUrl(
      title_, directory_resource_id_);
}

//============================ GetResourceEntryRequest =======================

GetResourceEntryRequest::GetResourceEntryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetDataCallback& callback)
    : GetDataRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      embed_origin_(embed_origin) {
  DCHECK(!callback.is_null());
}

GetResourceEntryRequest::~GetResourceEntryRequest() {}

GURL GetResourceEntryRequest::GetURL() const {
  return url_generator_.GenerateEditUrlWithEmbedOrigin(
      resource_id_, embed_origin_);
}

//========================= GetAccountMetadataRequest ========================

GetAccountMetadataRequest::GetAccountMetadataRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const GetAccountMetadataCallback& callback,
    bool include_installed_apps)
    : GetDataRequest(sender,
                     base::Bind(&ParseAccounetMetadataAndRun, callback)),
      url_generator_(url_generator),
      include_installed_apps_(include_installed_apps) {
  DCHECK(!callback.is_null());
}

GetAccountMetadataRequest::~GetAccountMetadataRequest() {}

GURL GetAccountMetadataRequest::GetURL() const {
  return url_generator_.GenerateAccountMetadataUrl(include_installed_apps_);
}

//=========================== DeleteResourceRequest ==========================

DeleteResourceRequest::DeleteResourceRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const std::string& resource_id,
    const std::string& etag)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      etag_(etag) {
  DCHECK(!callback.is_null());
}

DeleteResourceRequest::~DeleteResourceRequest() {}

GURL DeleteResourceRequest::GetURL() const {
  return url_generator_.GenerateEditUrl(resource_id_);
}

URLFetcher::RequestType DeleteResourceRequest::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
DeleteResourceRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

//========================== CreateDirectoryRequest ==========================

CreateDirectoryRequest::CreateDirectoryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const GetDataCallback& callback,
    const std::string& parent_resource_id,
    const std::string& directory_title)
    : GetDataRequest(sender, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      directory_title_(directory_title) {
  DCHECK(!callback.is_null());
}

CreateDirectoryRequest::~CreateDirectoryRequest() {}

GURL CreateDirectoryRequest::GetURL() const {
  return url_generator_.GenerateContentUrl(parent_resource_id_);
}

URLFetcher::RequestType
CreateDirectoryRequest::GetRequestType() const {
  return URLFetcher::POST;
}

bool CreateDirectoryRequest::GetContentData(std::string* upload_content_type,
                                            std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.StartElement("category");
  xml_writer.AddAttribute("scheme",
                          "http://schemas.google.com/g/2005#kind");
  xml_writer.AddAttribute("term",
                          "http://schemas.google.com/docs/2007#folder");
  xml_writer.EndElement();  // Ends "category" element.

  xml_writer.WriteElement("title", directory_title_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "CreateDirectory data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//============================ CopyHostedDocumentRequest =====================

CopyHostedDocumentRequest::CopyHostedDocumentRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const GetDataCallback& callback,
    const std::string& resource_id,
    const std::string& new_title)
    : GetDataRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      new_title_(new_title) {
  DCHECK(!callback.is_null());
}

CopyHostedDocumentRequest::~CopyHostedDocumentRequest() {}

URLFetcher::RequestType CopyHostedDocumentRequest::GetRequestType() const {
  return URLFetcher::POST;
}

GURL CopyHostedDocumentRequest::GetURL() const {
  return url_generator_.GenerateResourceListRootUrl();
}

bool CopyHostedDocumentRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement("id", resource_id_);
  xml_writer.WriteElement("title", new_title_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "CopyHostedDocumentRequest data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//=========================== RenameResourceRequest ==========================

RenameResourceRequest::RenameResourceRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const std::string& resource_id,
    const std::string& new_title)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      new_title_(new_title) {
  DCHECK(!callback.is_null());
}

RenameResourceRequest::~RenameResourceRequest() {}

URLFetcher::RequestType RenameResourceRequest::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string>
RenameResourceRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL RenameResourceRequest::GetURL() const {
  return url_generator_.GenerateEditUrl(resource_id_);
}

bool RenameResourceRequest::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement("title", new_title_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "RenameResourceRequest data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== AuthorizeAppRequest ==========================

AuthorizeAppRequest::AuthorizeAppRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const AuthorizeAppCallback& callback,
    const std::string& resource_id,
    const std::string& app_id)
    : GetDataRequest(sender,
                     base::Bind(&ParseOpenLinkAndRun, app_id, callback)),
      url_generator_(url_generator),
      resource_id_(resource_id),
      app_id_(app_id) {
  DCHECK(!callback.is_null());
}

AuthorizeAppRequest::~AuthorizeAppRequest() {}

URLFetcher::RequestType AuthorizeAppRequest::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string>
AuthorizeAppRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

bool AuthorizeAppRequest::GetContentData(std::string* upload_content_type,
                                         std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");
  xml_writer.AddAttribute("xmlns:docs", "http://schemas.google.com/docs/2007");
  xml_writer.WriteElement("docs:authorizedApp", app_id_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "AuthorizeAppRequest data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

GURL AuthorizeAppRequest::GetURL() const {
  return url_generator_.GenerateEditUrl(resource_id_);
}

//======================= AddResourceToDirectoryRequest ======================

AddResourceToDirectoryRequest::AddResourceToDirectoryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const std::string& parent_resource_id,
    const std::string& resource_id)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

AddResourceToDirectoryRequest::~AddResourceToDirectoryRequest() {}

GURL AddResourceToDirectoryRequest::GetURL() const {
  return url_generator_.GenerateContentUrl(parent_resource_id_);
}

URLFetcher::RequestType
AddResourceToDirectoryRequest::GetRequestType() const {
  return URLFetcher::POST;
}

bool AddResourceToDirectoryRequest::GetContentData(
    std::string* upload_content_type, std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement(
      "id", url_generator_.GenerateEditUrlWithoutParams(resource_id_).spec());

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "AddResourceToDirectoryRequest data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//==================== RemoveResourceFromDirectoryRequest ====================

RemoveResourceFromDirectoryRequest::RemoveResourceFromDirectoryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const std::string& parent_resource_id,
    const std::string& document_resource_id)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(document_resource_id),
      parent_resource_id_(parent_resource_id) {
  DCHECK(!callback.is_null());
}

RemoveResourceFromDirectoryRequest::~RemoveResourceFromDirectoryRequest() {
}

GURL RemoveResourceFromDirectoryRequest::GetURL() const {
  return url_generator_.GenerateResourceUrlForRemoval(
      parent_resource_id_, resource_id_);
}

URLFetcher::RequestType
RemoveResourceFromDirectoryRequest::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
RemoveResourceFromDirectoryRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

//======================= InitiateUploadNewFileRequest =======================

InitiateUploadNewFileRequest::InitiateUploadNewFileRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const InitiateUploadCallback& callback,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title)
    : InitiateUploadRequestBase(sender, callback, content_type, content_length),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      title_(title) {
}

InitiateUploadNewFileRequest::~InitiateUploadNewFileRequest() {}

GURL InitiateUploadNewFileRequest::GetURL() const {
  return url_generator_.GenerateInitiateUploadNewFileUrl(parent_resource_id_);
}

net::URLFetcher::RequestType
InitiateUploadNewFileRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool InitiateUploadNewFileRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");
  xml_writer.AddAttribute("xmlns:docs",
                          "http://schemas.google.com/docs/2007");
  xml_writer.WriteElement("title", title_);
  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "InitiateUploadNewFile: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//===================== InitiateUploadExistingFileRequest ====================

InitiateUploadExistingFileRequest::InitiateUploadExistingFileRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const InitiateUploadCallback& callback,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag)
    : InitiateUploadRequestBase(sender, callback, content_type, content_length),
      url_generator_(url_generator),
      resource_id_(resource_id),
      etag_(etag) {
}

InitiateUploadExistingFileRequest::~InitiateUploadExistingFileRequest() {}

GURL InitiateUploadExistingFileRequest::GetURL() const {
  return url_generator_.GenerateInitiateUploadExistingFileUrl(resource_id_);
}

net::URLFetcher::RequestType
InitiateUploadExistingFileRequest::GetRequestType() const {
  return net::URLFetcher::PUT;
}

bool InitiateUploadExistingFileRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  // According to the document there is no need to send the content-type.
  // However, the server would return 500 server error without the
  // content-type.
  // As its workaround, send "text/plain" content-type here.
  *upload_content_type = "text/plain";
  *upload_content = "";
  return true;
}

std::vector<std::string>
InitiateUploadExistingFileRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      InitiateUploadRequestBase::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

//============================ ResumeUploadRequest ===========================

ResumeUploadRequest::ResumeUploadRequest(
    RequestSender* sender,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback,
    const GURL& upload_location,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path)
    : ResumeUploadRequestBase(sender,
                              upload_location,
                              start_position,
                              end_position,
                              content_length,
                              content_type,
                              local_file_path),
      callback_(callback),
      progress_callback_(progress_callback) {
  DCHECK(!callback_.is_null());
}

ResumeUploadRequest::~ResumeUploadRequest() {}

void ResumeUploadRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response, scoped_ptr<base::Value> value) {
  callback_.Run(response, ParseResourceEntry(value.Pass()));
}

void ResumeUploadRequest::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

//========================== GetUploadStatusRequest ==========================

GetUploadStatusRequest::GetUploadStatusRequest(
    RequestSender* sender,
    const UploadRangeCallback& callback,
    const GURL& upload_url,
    int64 content_length)
    : GetUploadStatusRequestBase(sender, upload_url, content_length),
      callback_(callback) {
  DCHECK(!callback.is_null());
}

GetUploadStatusRequest::~GetUploadStatusRequest() {}

void GetUploadStatusRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response, scoped_ptr<base::Value> value) {
  callback_.Run(response, ParseResourceEntry(value.Pass()));
}

//========================== DownloadFileRequest ==========================

DownloadFileRequest::DownloadFileRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback,
    const std::string& resource_id,
    const base::FilePath& output_file_path)
    : DownloadFileRequestBase(
          sender,
          download_action_callback,
          get_content_callback,
          progress_callback,
          url_generator.GenerateDownloadFileUrl(resource_id),
          output_file_path) {
}

DownloadFileRequest::~DownloadFileRequest() {
}

}  // namespace google_apis
