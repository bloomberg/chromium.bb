// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operations.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"
#include "third_party/libxml/chromium/libxml_utils.h"

using net::URLFetcher;

namespace {

// etag matching header.
const char kIfMatchAllHeader[] = "If-Match: *";
const char kIfMatchHeaderFormat[] = "If-Match: %s";

// URL requesting documents list that belong to the authenticated user only
// (handled with '/-/mine' part).
const char kGetDocumentListURLForAllDocuments[] =
    "https://docs.google.com/feeds/default/private/full/-/mine";

// URL requesting documents list in a particular directory specified by "%s"
// that belong to the authenticated user only (handled with '/-/mine' part).
const char kGetDocumentListURLForDirectoryFormat[] =
    "https://docs.google.com/feeds/default/private/full/%s/contents/-/mine";

// URL requesting documents list of changes to documents collections.
const char kGetChangesListURL[] =
    "https://docs.google.com/feeds/default/private/changes";

// Root document list url.
const char kDocumentListRootURL[] =
    "https://docs.google.com/feeds/default/private/full";

// URL requesting single document entry whose resource id is specified by "%s".
const char kGetDocumentEntryURLFormat[] =
    "https://docs.google.com/feeds/default/private/full/%s";

// Metadata feed with things like user quota.
const char kAccountMetadataURL[] =
    "https://docs.google.com/feeds/metadata/default";

// URL requesting all contact groups.
const char kGetContactGroupsURL[] =
    "https://www.google.com/m8/feeds/groups/default/full?alt=json";

// URL requesting all contacts.
// TODO(derat): Per https://goo.gl/AufHP, "The feed may not contain all of the
// user's contacts, because there's a default limit on the number of results
// returned."  Decide if 10000 is reasonable or not.
const char kGetContactsURL[] =
    "https://www.google.com/m8/feeds/contacts/default/full"
    "?alt=json&showdeleted=true&max-results=10000";

// Query parameter optionally appended to |kGetContactsURL| to return contacts
// from a specific group (as opposed to all contacts).
const char kGetContactsGroupParam[] = "group";

// Query parameter optionally appended to |kGetContactsURL| to return only
// recently-updated contacts.
const char kGetContactsUpdatedMinParam[] = "updated-min";

const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";

#ifndef NDEBUG
// Use smaller 'page' size while debugging to ensure we hit feed reload
// almost always. Be careful not to use something too small on account that
// have many items because server side 503 error might kick in.
const int kMaxDocumentsPerFeed = 500;
const int kMaxDocumentsPerSearchFeed = 50;
#else
const int kMaxDocumentsPerFeed = 500;
const int kMaxDocumentsPerSearchFeed = 50;
#endif

const char kFeedField[] = "feed";

// Templates for file uploading.
const char kUploadParamConvertKey[] = "convert";
const char kUploadParamConvertValue[] = "false";
const char kUploadResponseLocation[] = "location";
const char kUploadResponseRange[] = "range";

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
GURL AddStandardUrlParams(const GURL& url) {
  GURL result =
      chrome_common_net::AppendOrReplaceQueryParameter(url, "v", "3");
  result =
      chrome_common_net::AppendOrReplaceQueryParameter(result, "alt", "json");
  return result;
}

// Adds additional parameters to metadata feed to include installed 3rd party
// applications.
GURL AddMetadataUrlParams(const GURL& url) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result, "include-installed-apps", "true");
  return result;
}

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
GURL AddFeedUrlParams(const GURL& url,
                      int num_items_to_fetch,
                      int changestamp,
                      const std::string& search_string) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result,
      "showfolders",
      "true");
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result,
      "max-results",
      base::StringPrintf("%d", num_items_to_fetch));
  result = chrome_common_net::AppendOrReplaceQueryParameter(
      result, "include-installed-apps", "true");

  if (changestamp) {
    result = chrome_common_net::AppendQueryParameter(
        result,
        "start-index",
        base::StringPrintf("%d", changestamp));
  }

  if (!search_string.empty()) {
    result = chrome_common_net::AppendOrReplaceQueryParameter(
        result, "q", search_string);
  }
  return result;
}

// Formats a URL for getting document list. If |directory_resource_id| is
// empty, returns a URL for fetching all documents. If it's given, returns a
// URL for fetching documents in a particular directory.
GURL FormatDocumentListURL(const std::string& directory_resource_id) {
  if (directory_resource_id.empty())
    return GURL(kGetDocumentListURLForAllDocuments);

  return GURL(base::StringPrintf(kGetDocumentListURLForDirectoryFormat,
                                 net::EscapePath(
                                     directory_resource_id).c_str()));
}

}  // namespace

namespace gdata {

//============================ Structs ===========================

ResumeUploadResponse::ResumeUploadResponse(GDataErrorCode code,
                                           int64 start_range_received,
                                           int64 end_range_received)
    : code(code),
      start_range_received(start_range_received),
      end_range_received(end_range_received) {
}

ResumeUploadResponse::~ResumeUploadResponse() {
}

InitiateUploadParams::InitiateUploadParams(
    UploadMode upload_mode,
    const std::string& title,
    const std::string& content_type,
    int64 content_length,
    const GURL& upload_location,
    const FilePath& virtual_path)
    : upload_mode(upload_mode),
      title(title),
      content_type(content_type),
      content_length(content_length),
      upload_location(upload_location),
      virtual_path(virtual_path) {
}

InitiateUploadParams::~InitiateUploadParams() {
}

ResumeUploadParams::ResumeUploadParams(
    UploadMode upload_mode,
    int64 start_range,
    int64 end_range,
    int64 content_length,
    const std::string& content_type,
    scoped_refptr<net::IOBuffer> buf,
    const GURL& upload_location,
    const FilePath& virtual_path) : upload_mode(upload_mode),
                                    start_range(start_range),
                                    end_range(end_range),
                                    content_length(content_length),
                                    content_type(content_type),
                                    buf(buf),
                                    upload_location(upload_location),
                                    virtual_path(virtual_path) {
}

ResumeUploadParams::~ResumeUploadParams() {
}

//============================ GetDocumentsOperation ===========================

GetDocumentsOperation::GetDocumentsOperation(
    OperationRegistry* registry,
    const GURL& url,
    int start_changestamp,
    const std::string& search_string,
    const std::string& directory_resource_id,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      override_url_(url),
      start_changestamp_(start_changestamp),
      search_string_(search_string),
      directory_resource_id_(directory_resource_id) {
}

GetDocumentsOperation::~GetDocumentsOperation() {}

GURL GetDocumentsOperation::GetURL() const {
  int max_docs = search_string_.empty() ? kMaxDocumentsPerFeed :
                                          kMaxDocumentsPerSearchFeed;

  if (!override_url_.is_empty())
    return AddFeedUrlParams(override_url_,
                            max_docs,
                            0,
                            search_string_);

  if (start_changestamp_ == 0) {
    return AddFeedUrlParams(FormatDocumentListURL(directory_resource_id_),
                            max_docs,
                            0,
                            search_string_);
  }

  return AddFeedUrlParams(GURL(kGetChangesListURL),
                          kMaxDocumentsPerFeed,
                          start_changestamp_,
                          std::string());
}

//============================ GetDocumentEntryOperation =======================

GetDocumentEntryOperation::GetDocumentEntryOperation(
    OperationRegistry* registry,
    const std::string& resource_id,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      resource_id_(resource_id) {
}

GetDocumentEntryOperation::~GetDocumentEntryOperation() {}

GURL GetDocumentEntryOperation::GetURL() const {
  GURL result = GURL(base::StringPrintf(kGetDocumentEntryURLFormat,
                                        net::EscapePath(resource_id_).c_str()));
  return AddStandardUrlParams(result);
}

//========================= GetAccountMetadataOperation ========================

GetAccountMetadataOperation::GetAccountMetadataOperation(
    OperationRegistry* registry,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback) {
}

GetAccountMetadataOperation::~GetAccountMetadataOperation() {}

GURL GetAccountMetadataOperation::GetURL() const {
  return AddMetadataUrlParams(GURL(kAccountMetadataURL));
}

//============================ DownloadFileOperation ===========================

DownloadFileOperation::DownloadFileOperation(
    OperationRegistry* registry,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const GURL& document_url,
    const FilePath& virtual_path,
    const FilePath& output_file_path)
    : UrlFetchOperationBase(registry,
                            OperationRegistry::OPERATION_DOWNLOAD,
                            virtual_path),
      download_action_callback_(download_action_callback),
      get_content_callback_(get_content_callback),
      document_url_(document_url) {
  // Make sure we download the content into a temp file.
  if (output_file_path.empty())
    save_temp_file_ = true;
  else
    output_file_path_ = output_file_path;
}

DownloadFileOperation::~DownloadFileOperation() {}

// Overridden from UrlFetchOperationBase.
GURL DownloadFileOperation::GetURL() const {
  return document_url_;
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

  if (!download_action_callback_.is_null())
    download_action_callback_.Run(code, document_url_, temp_file);
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void DownloadFileOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  if (!download_action_callback_.is_null())
    download_action_callback_.Run(code, document_url_, FilePath());
}

//=========================== DeleteDocumentOperation ==========================

DeleteDocumentOperation::DeleteDocumentOperation(
    OperationRegistry* registry,
    const EntryActionCallback& callback,
    const GURL& document_url)
    : EntryActionOperation(registry, callback, document_url) {
}

DeleteDocumentOperation::~DeleteDocumentOperation() {}

GURL DeleteDocumentOperation::GetURL() const {
  return AddStandardUrlParams(document_url());
}

URLFetcher::RequestType DeleteDocumentOperation::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
DeleteDocumentOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

//========================== CreateDirectoryOperation ==========================

CreateDirectoryOperation::CreateDirectoryOperation(
    OperationRegistry* registry,
    const GetDataCallback& callback,
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name)
    : GetDataOperation(registry, callback),
      parent_content_url_(parent_content_url),
      directory_name_(directory_name) {
}

CreateDirectoryOperation::~CreateDirectoryOperation() {}

GURL CreateDirectoryOperation::GetURL() const {
  if (!parent_content_url_.is_empty())
    return AddStandardUrlParams(parent_content_url_);

  return AddStandardUrlParams(GURL(kDocumentListRootURL));
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

//============================ CopyDocumentOperation ===========================

CopyDocumentOperation::CopyDocumentOperation(
    OperationRegistry* registry,
    const GetDataCallback& callback,
    const std::string& resource_id,
    const FilePath::StringType& new_name)
    : GetDataOperation(registry, callback),
      resource_id_(resource_id),
      new_name_(new_name) {
}

CopyDocumentOperation::~CopyDocumentOperation() {}

URLFetcher::RequestType CopyDocumentOperation::GetRequestType() const {
  return URLFetcher::POST;
}

GURL CopyDocumentOperation::GetURL() const {
  return AddStandardUrlParams(GURL(kDocumentListRootURL));
}

bool CopyDocumentOperation::GetContentData(std::string* upload_content_type,
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
  DVLOG(1) << "CopyDocumentOperation data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== RenameResourceOperation ==========================

RenameResourceOperation::RenameResourceOperation(
    OperationRegistry* registry,
    const EntryActionCallback& callback,
    const GURL& document_url,
    const FilePath::StringType& new_name)
    : EntryActionOperation(registry, callback, document_url),
      new_name_(new_name) {
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
  return AddStandardUrlParams(document_url());
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

AuthorizeAppsOperation::AuthorizeAppsOperation(
    OperationRegistry* registry,
    const GetDataCallback& callback,
    const GURL& document_url,
    const std::string& app_id)
    : GetDataOperation(registry, callback),
      app_id_(app_id),
      document_url_(document_url) {
}

AuthorizeAppsOperation::~AuthorizeAppsOperation() {}

URLFetcher::RequestType AuthorizeAppsOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string>
AuthorizeAppsOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}

void AuthorizeAppsOperation::ProcessURLFetchResults(const URLFetcher* source) {
  std::string data;
  source->GetResponseAsString(&data);
  GetDataOperation::ProcessURLFetchResults(source);
}

bool AuthorizeAppsOperation::GetContentData(std::string* upload_content_type,
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

void AuthorizeAppsOperation::ParseResponse(
    GDataErrorCode fetch_error_code,
    const std::string& data) {
  // Parse entry XML.
  XmlReader xml_reader;
  scoped_ptr<DocumentEntry> entry;
  if (xml_reader.Load(data)) {
    while (xml_reader.Read()) {
      if (xml_reader.NodeName() == DocumentEntry::GetEntryNodeName()) {
        entry.reset(DocumentEntry::CreateFromXml(&xml_reader));
        break;
      }
    }
  }

  // From the response, we create a list of the links returned, since those
  // are the only things we are interested in.
  scoped_ptr<base::ListValue> link_list(new ListValue);
  const ScopedVector<Link>& feed_links = entry->links();
  for (ScopedVector<Link>::const_iterator iter = feed_links.begin();
       iter != feed_links.end(); ++iter) {
    if ((*iter)->type() == Link::LINK_OPEN_WITH) {
      base::DictionaryValue* link = new DictionaryValue;
      link->SetString(std::string("href"), (*iter)->href().spec());
      link->SetString(std::string("mime_type"), (*iter)->mime_type());
      link->SetString(std::string("title"), (*iter)->title());
      link->SetString(std::string("app_id"), (*iter)->app_id());
      link_list->Append(link);
    }
  }

  RunCallback(fetch_error_code, link_list.PassAs<base::Value>());
  const bool success = true;
  OnProcessURLFetchResultsComplete(success);
}

GURL AuthorizeAppsOperation::GetURL() const {
  return document_url_;
}

//======================= AddResourceToDirectoryOperation ======================

AddResourceToDirectoryOperation::AddResourceToDirectoryOperation(
    OperationRegistry* registry,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& document_url)
    : EntryActionOperation(registry, callback, document_url),
      parent_content_url_(parent_content_url) {
}

AddResourceToDirectoryOperation::~AddResourceToDirectoryOperation() {}

GURL AddResourceToDirectoryOperation::GetURL() const {
  if (!parent_content_url_.is_empty())
    return AddStandardUrlParams(parent_content_url_);

  return AddStandardUrlParams(GURL(kDocumentListRootURL));
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

  xml_writer.WriteElement("id", document_url().spec());

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
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& document_url,
    const std::string& document_resource_id)
    : EntryActionOperation(registry, callback, document_url),
      resource_id_(document_resource_id),
      parent_content_url_(parent_content_url) {
}

RemoveResourceFromDirectoryOperation::~RemoveResourceFromDirectoryOperation() {
}

GURL RemoveResourceFromDirectoryOperation::GetURL() const {
  std::string escaped_resource_id = net::EscapePath(resource_id_);
  GURL edit_url(base::StringPrintf("%s/%s",
                                   parent_content_url_.spec().c_str(),
                                   escaped_resource_id.c_str()));
  return AddStandardUrlParams(edit_url);
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
    const InitiateUploadCallback& callback,
    const InitiateUploadParams& params)
    : UrlFetchOperationBase(registry,
                            OperationRegistry::OPERATION_UPLOAD,
                            params.virtual_path),
      callback_(callback),
      params_(params),
      initiate_upload_url_(chrome_common_net::AppendOrReplaceQueryParameter(
          params.upload_location,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
}

InitiateUploadOperation::~InitiateUploadOperation() {}

GURL InitiateUploadOperation::GetURL() const {
  return initiate_upload_url_;
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

  if (!callback_.is_null())
    callback_.Run(code, GURL(upload_location));
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void InitiateUploadOperation::NotifySuccessToOperationRegistry() {
  NotifySuspend();
}

void InitiateUploadOperation::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  if (!callback_.is_null())
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

  if (params_.upload_mode == UPLOAD_EXISTING_FILE)
    headers.push_back("If-Match: *");

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
    const ResumeUploadCallback& callback,
    const ResumeUploadParams& params)
  : UrlFetchOperationBase(registry,
                          OperationRegistry::OPERATION_UPLOAD,
                          params.virtual_path),
      callback_(callback),
      params_(params),
      last_chunk_completed_(false) {
}

ResumeUploadOperation::~ResumeUploadOperation() {}

GURL ResumeUploadOperation::GetURL() const {
  return params_.upload_location;
}

void ResumeUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
  net::HttpResponseHeaders* hdrs = source->GetResponseHeaders();
  int64 start_range_received = -1;
  int64 end_range_received = -1;
  scoped_ptr<DocumentEntry> entry;

  if (code == HTTP_RESUME_INCOMPLETE) {
    // Retrieve value of the first "Range" header.
    std::string range_received;
    hdrs->EnumerateHeader(NULL, kUploadResponseRange, &range_received);
    if (!range_received.empty()) {  // Parse the range header.
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_received, &ranges) &&
          !ranges.empty() ) {
        // We only care about the first start-end pair in the range.
        start_range_received = ranges[0].first_byte_position();
        end_range_received = ranges[0].last_byte_position();
      }
    }
    DVLOG(1) << "Got response for [" << params_.virtual_path.value()
             << "]: code=" << code
             << ", range_hdr=[" << range_received
             << "], range_parsed=" << start_range_received
             << "," << end_range_received;
  } else {
    // There might be explanation of unexpected error code in response.
    std::string response_content;
    source->GetResponseAsString(&response_content);
    DVLOG(1) << "Got response for [" << params_.virtual_path.value()
             << "]: code=" << code
             << ", content=[\n" << response_content << "\n]";

    // Parse entry XML.
    XmlReader xml_reader;
    if (xml_reader.Load(response_content)) {
      while (xml_reader.Read()) {
        if (xml_reader.NodeName() == DocumentEntry::GetEntryNodeName()) {
          entry.reset(DocumentEntry::CreateFromXml(&xml_reader));
          break;
        }
      }
    }
    if (!entry.get())
      LOG(WARNING) << "Invalid entry received on upload:\n" << response_content;
  }

  if (!callback_.is_null()) {
    callback_.Run(ResumeUploadResponse(code,
                                       start_range_received,
                                       end_range_received),
                  entry.Pass());
  }

  // For a new file, HTTP_CREATED is returned.
  // For an existing file, HTTP_SUCCESS is returned.
  if ((params_.upload_mode == UPLOAD_NEW_FILE && code == HTTP_CREATED) ||
      (params_.upload_mode == UPLOAD_EXISTING_FILE && code == HTTP_SUCCESS)) {
    last_chunk_completed_ = true;
  }

  OnProcessURLFetchResultsComplete(
      last_chunk_completed_ || code == HTTP_RESUME_INCOMPLETE);
}

void ResumeUploadOperation::NotifyStartToOperationRegistry() {
  NotifyResume();
}

void ResumeUploadOperation::NotifySuccessToOperationRegistry() {
  if (last_chunk_completed_)
    NotifyFinish(OperationRegistry::OPERATION_COMPLETED);
  else
    NotifySuspend();
}

void ResumeUploadOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  scoped_ptr<DocumentEntry> entry;
  if (!callback_.is_null())
    callback_.Run(ResumeUploadResponse(code, 0, 0), entry.Pass());
}

URLFetcher::RequestType ResumeUploadOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string> ResumeUploadOperation::GetExtraRequestHeaders() const {
  if (params_.content_length == 0) {
    // For uploading an empty document, just PUT an empty content.
    DCHECK_EQ(params_.start_range, 0);
    DCHECK_EQ(params_.end_range, -1);
    return std::vector<std::string>();
  }

  // The header looks like
  // Content-Range: bytes <start_range>-<end_range>/<content_length>
  // for example:
  // Content-Range: bytes 7864320-8388607/13851821
  // Use * for unknown/streaming content length.
  DCHECK_GE(params_.start_range, 0);
  DCHECK_GE(params_.end_range, 0);
  DCHECK_GE(params_.content_length, -1);

  std::vector<std::string> headers;
  headers.push_back(
      std::string(kUploadContentRange) +
      base::Int64ToString(params_.start_range) + "-" +
      base::Int64ToString(params_.end_range) + "/" +
      (params_.content_length == -1 ? "*" :
          base::Int64ToString(params_.content_length)));
  return headers;
}

bool ResumeUploadOperation::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = params_.content_type;
  *upload_content = std::string(params_.buf->data(),
                                params_.end_range - params_.start_range + 1);
  return true;
}

void ResumeUploadOperation::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  // Adjust the progress values according to the range currently uploaded.
  NotifyProgress(params_.start_range + current, params_.content_length);
}

//========================== GetContactGroupsOperation =========================

GetContactGroupsOperation::GetContactGroupsOperation(
    OperationRegistry* registry,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback) {
}

GetContactGroupsOperation::~GetContactGroupsOperation() {}

GURL GetContactGroupsOperation::GetURL() const {
  return !feed_url_for_testing_.is_empty() ?
         feed_url_for_testing_ :
         GURL(kGetContactGroupsURL);
}

//============================ GetContactsOperation ============================

GetContactsOperation::GetContactsOperation(OperationRegistry* registry,
                                           const std::string& group_id,
                                           const base::Time& min_update_time,
                                           const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      group_id_(group_id),
      min_update_time_(min_update_time) {
}

GetContactsOperation::~GetContactsOperation() {}

GURL GetContactsOperation::GetURL() const {
  if (!feed_url_for_testing_.is_empty())
    return GURL(feed_url_for_testing_);

  GURL url(kGetContactsURL);

  if (!group_id_.empty()) {
    url = chrome_common_net::AppendQueryParameter(
              url, kGetContactsGroupParam, group_id_);
  }
  if (!min_update_time_.is_null()) {
    std::string time_rfc3339 = util::FormatTimeAsString(min_update_time_);
    url = chrome_common_net::AppendQueryParameter(
              url, kGetContactsUpdatedMinParam, time_rfc3339);
  }
  return url;
}

//========================== GetContactPhotoOperation ==========================

GetContactPhotoOperation::GetContactPhotoOperation(
    OperationRegistry* registry,
    const GURL& photo_url,
    const GetContentCallback& callback)
    : UrlFetchOperationBase(registry),
      photo_url_(photo_url),
      callback_(callback) {
}

GetContactPhotoOperation::~GetContactPhotoOperation() {}

GURL GetContactPhotoOperation::GetURL() const {
  return photo_url_;
}

void GetContactPhotoOperation::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());
  scoped_ptr<std::string> data(new std::string);
  source->GetResponseAsString(data.get());
  callback_.Run(code, data.Pass());
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void GetContactPhotoOperation::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  scoped_ptr<std::string> data(new std::string);
  callback_.Run(code, data.Pass());
}

}  // namespace gdata
