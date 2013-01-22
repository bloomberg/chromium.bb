// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_operations.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "third_party/libxml/chromium/libxml_utils.h"

using content::BrowserThread;
using net::URLFetcher;

namespace {

// etag matching header.
const char kIfMatchAllHeader[] = "If-Match: *";
const char kIfMatchHeaderFormat[] = "If-Match: %s";

const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";

const char kFeedField[] = "feed";

// Templates for file uploading.
const char kUploadParamConvertKey[] = "convert";
const char kUploadParamConvertValue[] = "false";
const char kUploadResponseLocation[] = "location";
const char kUploadResponseRange[] = "range";

}  // namespace

namespace google_apis {

//============================ Structs ===========================

ResumeUploadResponse::ResumeUploadResponse()
    : code(HTTP_SUCCESS),
      start_position_received(0),
      end_position_received(0) {
}

ResumeUploadResponse::ResumeUploadResponse(GDataErrorCode code,
                                           int64 start_position_received,
                                           int64 end_position_received)
    : code(code),
      start_position_received(start_position_received),
      end_position_received(end_position_received) {
}

ResumeUploadResponse::~ResumeUploadResponse() {
}

InitiateUploadParams::InitiateUploadParams(
    UploadMode upload_mode,
    const std::string& title,
    const std::string& content_type,
    int64 content_length,
    const GURL& upload_location,
    const FilePath& drive_file_path,
    const std::string& etag)
    : upload_mode(upload_mode),
      title(title),
      content_type(content_type),
      content_length(content_length),
      upload_location(upload_location),
      drive_file_path(drive_file_path),
      etag(etag) {
}

InitiateUploadParams::~InitiateUploadParams() {
}

ResumeUploadParams::ResumeUploadParams(
    UploadMode upload_mode,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    scoped_refptr<net::IOBuffer> buf,
    const GURL& upload_location,
    const FilePath& drive_file_path) : upload_mode(upload_mode),
                                    start_position(start_position),
                                    end_position(end_position),
                                    content_length(content_length),
                                    content_type(content_type),
                                    buf(buf),
                                    upload_location(upload_location),
                                    drive_file_path(drive_file_path) {
}

ResumeUploadParams::~ResumeUploadParams() {
}

//============================ GetResourceListOperation ========================

GetResourceListOperation::GetResourceListOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const GURL& override_url,
    int start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      override_url_(override_url),
      start_changestamp_(start_changestamp),
      search_string_(search_string),
      shared_with_me_(shared_with_me),
      directory_resource_id_(directory_resource_id) {
  DCHECK(!callback.is_null());
}

GetResourceListOperation::~GetResourceListOperation() {}

GURL GetResourceListOperation::GetURL() const {
  return url_generator_.GenerateResourceListUrl(override_url_,
                                                start_changestamp_,
                                                search_string_,
                                                shared_with_me_,
                                                directory_resource_id_);
}

//============================ GetResourceEntryOperation =======================

GetResourceEntryOperation::GetResourceEntryOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const std::string& resource_id,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

GetResourceEntryOperation::~GetResourceEntryOperation() {}

GURL GetResourceEntryOperation::GetURL() const {
  return url_generator_.GenerateResourceEntryUrl(resource_id_);
}

//========================= GetAccountMetadataOperation ========================

GetAccountMetadataOperation::GetAccountMetadataOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const GetDataCallback& callback)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

GetAccountMetadataOperation::~GetAccountMetadataOperation() {}

GURL GetAccountMetadataOperation::GetURL() const {
  return url_generator_.GenerateAccountMetadataUrl();
}

//============================ DownloadFileOperation ===========================

DownloadFileOperation::DownloadFileOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const GURL& content_url,
    const FilePath& drive_file_path,
    const FilePath& output_file_path)
    : UrlFetchOperationBase(registry,
                            url_request_context_getter,
                            OPERATION_DOWNLOAD,
                            drive_file_path),
      download_action_callback_(download_action_callback),
      get_content_callback_(get_content_callback),
      content_url_(content_url) {
  DCHECK(!download_action_callback_.is_null());
  // get_content_callback may be null.

  // Make sure we download the content into a temp file.
  if (output_file_path.empty())
    set_save_temp_file(true);
  else
    set_output_file_path(output_file_path);
}

DownloadFileOperation::~DownloadFileOperation() {}

// Overridden from UrlFetchOperationBase.
GURL DownloadFileOperation::GetURL() const {
  return content_url_;
}

void DownloadFileOperation::OnURLFetchDownloadProgress(const URLFetcher* source,
                                                       int64 current,
                                                       int64 total) {
  NotifyProgress(current, total);
}

bool DownloadFileOperation::ShouldSendDownloadData() {
  return !get_content_callback_.is_null();
}

void DownloadFileOperation::OnURLFetchDownloadData(
    const URLFetcher* source,
    scoped_ptr<std::string> download_data) {
  if (!get_content_callback_.is_null())
    get_content_callback_.Run(HTTP_SUCCESS, download_data.Pass());
}

void DownloadFileOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);

  // Take over the ownership of the the downloaded temp file.
  FilePath temp_file;
  if (code == HTTP_SUCCESS &&
      !source->GetResponseAsFilePath(true,  // take_ownership
                                     &temp_file)) {
    code = GDATA_FILE_ERROR;
  }

  download_action_callback_.Run(code, temp_file);
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void DownloadFileOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  download_action_callback_.Run(code, FilePath());
}

//=========================== DeleteResourceOperation ==========================

DeleteResourceOperation::DeleteResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const EntryActionCallback& callback,
    const GURL& edit_url)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      edit_url_(edit_url) {
  DCHECK(!callback.is_null());
}

DeleteResourceOperation::~DeleteResourceOperation() {}

GURL DeleteResourceOperation::GetURL() const {
  return GDataWapiUrlGenerator::AddStandardUrlParams(edit_url_);
}

URLFetcher::RequestType DeleteResourceOperation::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
DeleteResourceOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

//========================== CreateDirectoryOperation ==========================

CreateDirectoryOperation::CreateDirectoryOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const GetDataCallback& callback,
    const GURL& parent_content_url,
    const std::string& directory_name)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      parent_content_url_(parent_content_url),
      directory_name_(directory_name) {
  DCHECK(!callback.is_null());
}

CreateDirectoryOperation::~CreateDirectoryOperation() {}

GURL CreateDirectoryOperation::GetURL() const {
  if (!parent_content_url_.is_empty())
    return GDataWapiUrlGenerator::AddStandardUrlParams(parent_content_url_);

  return url_generator_.GenerateResourceListRootUrl();
}

URLFetcher::RequestType
CreateDirectoryOperation::GetRequestType() const {
  return URLFetcher::POST;
}

bool CreateDirectoryOperation::GetContentData(std::string* upload_content_type,
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

  xml_writer.WriteElement("title", directory_name_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "CreateDirectory data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//============================ CopyHostedDocumentOperation =====================

CopyHostedDocumentOperation::CopyHostedDocumentOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const GetDataCallback& callback,
    const std::string& resource_id,
    const std::string& new_name)
    : GetDataOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      new_name_(new_name) {
  DCHECK(!callback.is_null());
}

CopyHostedDocumentOperation::~CopyHostedDocumentOperation() {}

URLFetcher::RequestType CopyHostedDocumentOperation::GetRequestType() const {
  return URLFetcher::POST;
}

GURL CopyHostedDocumentOperation::GetURL() const {
  return url_generator_.GenerateResourceListRootUrl();
}

bool CopyHostedDocumentOperation::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement("id", resource_id_);
  xml_writer.WriteElement("title", new_name_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "CopyHostedDocumentOperation data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//=========================== RenameResourceOperation ==========================

RenameResourceOperation::RenameResourceOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const EntryActionCallback& callback,
    const GURL& edit_url,
    const std::string& new_name)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      edit_url_(edit_url),
      new_name_(new_name) {
  DCHECK(!callback.is_null());
}

RenameResourceOperation::~RenameResourceOperation() {}

URLFetcher::RequestType RenameResourceOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string>
RenameResourceOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

GURL RenameResourceOperation::GetURL() const {
  return GDataWapiUrlGenerator::AddStandardUrlParams(edit_url_);
}

bool RenameResourceOperation::GetContentData(std::string* upload_content_type,
                                             std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement("title", new_name_);

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "RenameResourceOperation data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== AuthorizeAppOperation ==========================

AuthorizeAppOperation::AuthorizeAppOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GetDataCallback& callback,
    const GURL& edit_url,
    const std::string& app_id)
    : GetDataOperation(registry, url_request_context_getter, callback),
      app_id_(app_id),
      edit_url_(edit_url) {
  DCHECK(!callback.is_null());
}

AuthorizeAppOperation::~AuthorizeAppOperation() {}

URLFetcher::RequestType AuthorizeAppOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string>
AuthorizeAppOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

bool AuthorizeAppOperation::GetContentData(std::string* upload_content_type,
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
  DVLOG(1) << "AuthorizeAppOperation data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

GURL AuthorizeAppOperation::GetURL() const {
  return GDataWapiUrlGenerator::AddStandardUrlParams(edit_url_);
}

//======================= AddResourceToDirectoryOperation ======================

AddResourceToDirectoryOperation::AddResourceToDirectoryOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& edit_url)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      parent_content_url_(parent_content_url),
      edit_url_(edit_url) {
  DCHECK(!callback.is_null());
}

AddResourceToDirectoryOperation::~AddResourceToDirectoryOperation() {}

GURL AddResourceToDirectoryOperation::GetURL() const {
  GURL parent = parent_content_url_.is_empty() ?
      url_generator_.GenerateRootContentUrl() : parent_content_url_;
  return GDataWapiUrlGenerator::AddStandardUrlParams(parent);
}

URLFetcher::RequestType
AddResourceToDirectoryOperation::GetRequestType() const {
  return URLFetcher::POST;
}

bool AddResourceToDirectoryOperation::GetContentData(
    std::string* upload_content_type, std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");

  xml_writer.WriteElement("id", edit_url_.spec());

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "AddResourceToDirectoryOperation data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//==================== RemoveResourceFromDirectoryOperation ====================

RemoveResourceFromDirectoryOperation::RemoveResourceFromDirectoryOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GDataWapiUrlGenerator& url_generator,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const std::string& document_resource_id)
    : EntryActionOperation(registry, url_request_context_getter, callback),
      url_generator_(url_generator),
      resource_id_(document_resource_id),
      parent_content_url_(parent_content_url) {
  DCHECK(!callback.is_null());
}

RemoveResourceFromDirectoryOperation::~RemoveResourceFromDirectoryOperation() {
}

GURL RemoveResourceFromDirectoryOperation::GetURL() const {
  GURL parent = parent_content_url_.is_empty() ?
      url_generator_.GenerateRootContentUrl() : parent_content_url_;

  std::string escaped_resource_id = net::EscapePath(resource_id_);
  GURL edit_url(base::StringPrintf("%s/%s",
                                   parent.spec().c_str(),
                                   escaped_resource_id.c_str()));
  return GDataWapiUrlGenerator::AddStandardUrlParams(edit_url);
}

URLFetcher::RequestType
RemoveResourceFromDirectoryOperation::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
RemoveResourceFromDirectoryOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

//=========================== InitiateUploadOperation ==========================

InitiateUploadOperation::InitiateUploadOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const InitiateUploadCallback& callback,
    const InitiateUploadParams& params)
    : UrlFetchOperationBase(registry,
                            url_request_context_getter,
                            OPERATION_UPLOAD,
                            params.drive_file_path),
      callback_(callback),
      params_(params),
      initiate_upload_url_(chrome_common_net::AppendOrReplaceQueryParameter(
          params.upload_location,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
  DCHECK(!callback_.is_null());
}

InitiateUploadOperation::~InitiateUploadOperation() {}

GURL InitiateUploadOperation::GetURL() const {
  return GDataWapiUrlGenerator::AddStandardUrlParams(initiate_upload_url_);
}

void InitiateUploadOperation::ProcessURLFetchResults(
    const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);

  std::string upload_location;
  if (code == HTTP_SUCCESS) {
    // Retrieve value of the first "Location" header.
    source->GetResponseHeaders()->EnumerateHeader(NULL,
                                                  kUploadResponseLocation,
                                                  &upload_location);
  }
  VLOG(1) << "Got response for [" << params_.title
          << "]: code=" << code
          << ", location=[" << upload_location << "]";

  callback_.Run(code, GURL(upload_location));
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void InitiateUploadOperation::NotifySuccessToOperationRegistry() {
  NotifySuspend();
}

void InitiateUploadOperation::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  callback_.Run(code, GURL());
}

URLFetcher::RequestType InitiateUploadOperation::GetRequestType() const {
  if (params_.upload_mode == UPLOAD_NEW_FILE)
    return URLFetcher::POST;

  DCHECK_EQ(UPLOAD_EXISTING_FILE, params_.upload_mode);
  return URLFetcher::PUT;
}

std::vector<std::string>
InitiateUploadOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  if (!params_.content_type.empty())
    headers.push_back(kUploadContentType + params_.content_type);

  headers.push_back(
      kUploadContentLength + base::Int64ToString(params_.content_length));

  if (params_.upload_mode == UPLOAD_EXISTING_FILE) {
    if (params_.etag.empty()) {
      headers.push_back(kIfMatchAllHeader);
    } else {
      headers.push_back(
          StringPrintf(kIfMatchHeaderFormat, params_.etag.c_str()));
    }
  }

  return headers;
}

bool InitiateUploadOperation::GetContentData(std::string* upload_content_type,
                                             std::string* upload_content) {
  if (params_.upload_mode == UPLOAD_EXISTING_FILE) {
    // When uploading an existing file, the body is empty as we don't modify
    // the metadata.
    *upload_content = "";
    // Even though the body is empty, Content-Type should be set to
    // "text/plain". Otherwise, the server won't accept.
    *upload_content_type = "text/plain";
    return true;
  }

  DCHECK_EQ(UPLOAD_NEW_FILE, params_.upload_mode);
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");
  xml_writer.AddAttribute("xmlns:docs",
                          "http://schemas.google.com/docs/2007");
  xml_writer.WriteElement("title", params_.title);
  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "Upload data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//============================ ResumeUploadOperation ===========================

ResumeUploadOperation::ResumeUploadOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const ResumeUploadCallback& callback,
    const ResumeUploadParams& params)
  : UrlFetchOperationBase(registry,
                          url_request_context_getter,
                          OPERATION_UPLOAD,
                          params.drive_file_path),
      callback_(callback),
      params_(params),
      last_chunk_completed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(!callback_.is_null());
}

ResumeUploadOperation::~ResumeUploadOperation() {}

GURL ResumeUploadOperation::GetURL() const {
  // This is very tricky to get json from this operation. To do that, &alt=json
  // has to be appended not here but in InitiateUploadOperation::GetURL().
  return params_.upload_location;
}

void ResumeUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
  net::HttpResponseHeaders* hdrs = source->GetResponseHeaders();

  if (code == HTTP_RESUME_INCOMPLETE) {
    // Retrieve value of the first "Range" header.
    int64 start_position_received = -1;
    int64 end_position_received = -1;
    std::string range_received;
    hdrs->EnumerateHeader(NULL, kUploadResponseRange, &range_received);
    if (!range_received.empty()) {  // Parse the range header.
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_received, &ranges) &&
          !ranges.empty() ) {
        // We only care about the first start-end pair in the range.
        //
        // Range header represents the range inclusively, while we are treating
        // ranges exclusively (i.e., end_position_received should be one passed
        // the last valid index). So "+ 1" is added.
        start_position_received = ranges[0].first_byte_position();
        end_position_received = ranges[0].last_byte_position() + 1;
      }
    }
    DVLOG(1) << "Got response for [" << params_.drive_file_path.value()
             << "]: code=" << code
             << ", range_hdr=[" << range_received
             << "], range_parsed=" << start_position_received
             << "," << end_position_received;

    callback_.Run(ResumeUploadResponse(code,
                                       start_position_received,
                                       end_position_received),
                  scoped_ptr<ResourceEntry>());

    OnProcessURLFetchResultsComplete(true);
  } else {
    // There might be explanation of unexpected error code in response.
    std::string response_content;
    source->GetResponseAsString(&response_content);
    DVLOG(1) << "Got response for [" << params_.drive_file_path.value()
             << "]: code=" << code
             << ", content=[\n" << response_content << "\n]";

    ParseJson(response_content,
              base::Bind(&ResumeUploadOperation::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr(),
                         code));
  }
}

void ResumeUploadOperation::OnDataParsed(GDataErrorCode code,
                                         scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // For a new file, HTTP_CREATED is returned.
  // For an existing file, HTTP_SUCCESS is returned.
  if ((params_.upload_mode == UPLOAD_NEW_FILE && code == HTTP_CREATED) ||
      (params_.upload_mode == UPLOAD_EXISTING_FILE && code == HTTP_SUCCESS)) {
    last_chunk_completed_ = true;
  }

  scoped_ptr<ResourceEntry> entry;
  if (value.get())
    entry = ResourceEntry::ExtractAndParse(*(value.get()));

  if (!entry.get())
    LOG(WARNING) << "Invalid entry received on upload.";

  callback_.Run(ResumeUploadResponse(code, -1, -1), entry.Pass());
  OnProcessURLFetchResultsComplete(last_chunk_completed_);
}

void ResumeUploadOperation::NotifyStartToOperationRegistry() {
  NotifyResume();
}

void ResumeUploadOperation::NotifySuccessToOperationRegistry() {
  if (last_chunk_completed_)
    NotifyFinish(OPERATION_COMPLETED);
  else
    NotifySuspend();
}

void ResumeUploadOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  scoped_ptr<ResourceEntry> entry;
  callback_.Run(ResumeUploadResponse(code, 0, 0), entry.Pass());
}

URLFetcher::RequestType ResumeUploadOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string> ResumeUploadOperation::GetExtraRequestHeaders() const {
  if (params_.content_length == 0) {
    // For uploading an empty document, just PUT an empty content.
    DCHECK_EQ(params_.start_position, 0);
    DCHECK_EQ(params_.end_position, 0);
    return std::vector<std::string>();
  }

  // The header looks like
  // Content-Range: bytes <start_position>-<end_position>/<content_length>
  // for example:
  // Content-Range: bytes 7864320-8388607/13851821
  // Use * for unknown/streaming content length.
  // The header takes inclusive range, so we adjust by "end_position - 1".
  DCHECK_GE(params_.start_position, 0);
  DCHECK_GT(params_.end_position, 0);
  DCHECK_GE(params_.content_length, -1);

  std::vector<std::string> headers;
  headers.push_back(
      std::string(kUploadContentRange) +
      base::Int64ToString(params_.start_position) + "-" +
      base::Int64ToString(params_.end_position - 1) + "/" +
      (params_.content_length == -1 ? "*" :
          base::Int64ToString(params_.content_length)));
  return headers;
}

bool ResumeUploadOperation::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = params_.content_type;
  *upload_content = std::string(params_.buf->data(),
                                params_.end_position - params_.start_position);
  return true;
}

void ResumeUploadOperation::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  // Adjust the progress values according to the range currently uploaded.
  NotifyProgress(params_.start_position + current, params_.content_length);
}

}  // namespace google_apis
