// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operations.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"

using content::BrowserThread;
using content::URLFetcher;

namespace {

// etag matching header.
const char kIfMatchAllHeader[] = "If-Match: *";
const char kIfMatchHeaderFormat[] = "If-Match: %s";

// URL requesting documents list that belong to the authenticated user only
// (handled with '-/mine' part).
const char kGetDocumentListURL[] =
    "https://docs.google.com/feeds/default/private/full/-/mine";

// Root document list url.
const char kDocumentListRootURL[] =
    "https://docs.google.com/feeds/default/private/full";

// Metadata feed with things like user quota.
const char kAccountMetadataURL[] =
    "https://docs.google.com/feeds/metadata/default";

const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";

#ifndef NDEBUG
// Use smaller 'page' size while debugging to ensure we hit feed reload
// almost always. Be careful not to use something too small on account that
// have many items because server side 503 error might kick in.
const int kMaxDocumentsPerFeed = 1000;
#else
const int kMaxDocumentsPerFeed = 1000;
#endif

const char kFeedField[] = "feed";

// Templates for file uploading.
const char kUploadParamConvertKey[] = "convert";
const char kUploadParamConvertValue[] = "false";
const char kUploadResponseLocation[] = "location";
const char kUploadResponseRange[] = "range";

// OAuth scope for the documents API.
const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
GURL AddStandardUrlParams(const GURL& url) {
  GURL result = chrome_browser_net::AppendQueryParameter(url, "v", "3");
  result = chrome_browser_net::AppendQueryParameter(result, "alt", "json");
  return result;
}

// Adds additional parameters for API version, output content type and to show
// folders in the feed are added to document feed URLs.
GURL AddFeedUrlParams(const GURL& url) {
  GURL result = AddStandardUrlParams(url);
  result = chrome_browser_net::AppendQueryParameter(result,
                                                    "showfolders",
                                                    "true");
  result = chrome_browser_net::AppendQueryParameter(
      result,
      "max-results",
      base::StringPrintf("%d", kMaxDocumentsPerFeed));
  return result;
}

}  // namespace

namespace gdata {

//================================ AuthOperation ===============================

AuthOperation::AuthOperation(GDataOperationRegistry* registry,
                             Profile* profile,
                             const AuthStatusCallback& callback,
                             const std::string& refresh_token)
    : GDataOperationRegistry::Operation(registry),
      profile_(profile), token_(refresh_token), callback_(callback) {
}

AuthOperation::~AuthOperation() {}

void AuthOperation::Start() {
  DCHECK(!token_.empty());
  std::vector<std::string> scopes;
  scopes.push_back(kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, profile_->GetRequestContext()));
  NotifyStart();
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      token_,
      scopes);
}

void AuthOperation::DoCancel() {
  oauth2_access_token_fetcher_->CancelRequest();
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void AuthOperation::OnGetTokenSuccess(const std::string& access_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_.Run(HTTP_SUCCESS, access_token);
  NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "AuthOperation: token request using refresh token failed";
  callback_.Run(HTTP_UNAUTHORIZED, std::string());
  NotifyFinish(GDataOperationRegistry::OPERATION_FAILED);
}

//============================ EntryActionOperation ============================

EntryActionOperation::EntryActionOperation(GDataOperationRegistry* registry,
                                           Profile* profile,
                                           const EntryActionCallback& callback,
                                           const GURL& document_url)
    : UrlFetchOperation<EntryActionCallback>(registry, profile, callback),
      document_url_(document_url) {
}

EntryActionOperation::~EntryActionOperation() {}

// Overridden from UrlFetchOperation.
GURL EntryActionOperation::GetURL() const {
  return AddStandardUrlParams(document_url_);
}

void EntryActionOperation::ProcessURLFetchResults(const URLFetcher* source) {
  if (!callback_.is_null()) {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    relay_proxy_->PostTask(FROM_HERE,
                           base::Bind(callback_, code, document_url_));
  }
}

void EntryActionOperation::RunCallbackOnAuthFailed(GDataErrorCode code) {
  if (!callback_.is_null()) {
    relay_proxy_->PostTask(FROM_HERE,
                           base::Bind(callback_, code, document_url_));
  }
}

//============================== GetDataOperation ==============================

GetDataOperation::GetDataOperation(GDataOperationRegistry* registry,
                                   Profile* profile,
                                   const GetDataCallback& callback)
    : UrlFetchOperation<GetDataCallback>(registry, profile, callback) {
}

GetDataOperation::~GetDataOperation() {}

void GetDataOperation::ProcessURLFetchResults(const URLFetcher* source) {
  std::string data;
  source->GetResponseAsString(&data);
  scoped_ptr<base::Value> root_value;
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());

  switch (code) {
    case HTTP_SUCCESS:
    case HTTP_CREATED: {
      root_value.reset(ParseResponse(data));
      if (!root_value.get())
        code = GDATA_PARSE_ERROR;

      break;
    }
    default:
      break;
  }

  if (!callback_.is_null()) {
    relay_proxy_->PostTask(
        FROM_HERE,
        base::Bind(callback_, code, base::Passed(&root_value)));
  }
}

void GetDataOperation::RunCallbackOnAuthFailed(GDataErrorCode code) {
  if (!callback_.is_null()) {
    scoped_ptr<base::Value> root_value;
    relay_proxy_->PostTask(
        FROM_HERE,
        base::Bind(callback_, code, base::Passed(&root_value)));
  }
}

// static
base::Value* GetDataOperation::ParseResponse(const std::string& data) {
  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
      data, false, &error_code, &error_message));
  if (!root_value.get()) {
    LOG(ERROR) << "Error while parsing entry response: "
               << error_message
               << ", code: "
               << error_code
               << ", data:\n"
               << data;
    return NULL;
  }
  return root_value.release();
}

//============================ GetDocumentsOperation ===========================

GetDocumentsOperation::GetDocumentsOperation(GDataOperationRegistry* registry,
                                             Profile* profile,
                                             const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback) {
}

GetDocumentsOperation::~GetDocumentsOperation() {}

void GetDocumentsOperation::SetUrl(const GURL& url) {
  override_url_ = url;
}

GURL GetDocumentsOperation::GetURL() const {
  if (!override_url_.is_empty())
    return AddFeedUrlParams(override_url_);

  return AddFeedUrlParams(GURL(kGetDocumentListURL));
}

//========================= GetAccountMetadataOperation ========================

GetAccountMetadataOperation::GetAccountMetadataOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback) {
}

GetAccountMetadataOperation::~GetAccountMetadataOperation() {}

GURL GetAccountMetadataOperation::GetURL() const {
  return AddStandardUrlParams(GURL(kAccountMetadataURL));
}

//============================ DownloadFileOperation ===========================

DownloadFileOperation::DownloadFileOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const DownloadActionCallback& callback,
    const GURL& document_url)
    : UrlFetchOperation<DownloadActionCallback>(registry, profile, callback),
      document_url_(document_url) {
  // Make sure we download the content into a temp file.
  save_temp_file_ = true;
}

DownloadFileOperation::~DownloadFileOperation() {}

// Overridden from UrlFetchOperation.
GURL DownloadFileOperation::GetURL() const {
  return document_url_;
}

void DownloadFileOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());

  // Take over the ownership of the the downloaded temp file.
  FilePath temp_file;
  if (code == HTTP_SUCCESS &&
      !source->GetResponseAsFilePath(true,  // take_ownership
                                     &temp_file)) {
    code = GDATA_FILE_ERROR;
  }

  if (!callback_.is_null()) {
    relay_proxy_->PostTask(
        FROM_HERE,
        base::Bind(callback_, code, document_url_, temp_file));
  }
}

void DownloadFileOperation::RunCallbackOnAuthFailed(GDataErrorCode code) {
  if (!callback_.is_null()) {
    relay_proxy_->PostTask(
        FROM_HERE,
        base::Bind(callback_, code, document_url_, FilePath()));
  }
}

//=========================== DeleteDocumentOperation ==========================

DeleteDocumentOperation::DeleteDocumentOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& document_url)
    : EntryActionOperation(registry, profile, callback, document_url) {
}

DeleteDocumentOperation::~DeleteDocumentOperation() {}

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
    GDataOperationRegistry* registry,
    Profile* profile,
    const GetDataCallback& callback,
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name)
    : GetDataOperation(registry, profile, callback),
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
    GDataOperationRegistry* registry,
    Profile* profile,
    const GetDataCallback& callback,
    const GURL& document_url,
    const FilePath::StringType& new_name)
    : GetDataOperation(registry, profile, callback),
      document_url_(document_url),
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

  xml_writer.WriteElement("id", document_url_.spec());
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
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& document_url,
    const FilePath::StringType& new_name)
    : EntryActionOperation(registry, profile, callback, document_url),
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

//======================= AddResourceToDirectoryOperation ======================

AddResourceToDirectoryOperation::AddResourceToDirectoryOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& document_url)
    : EntryActionOperation(registry, profile, callback, document_url),
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
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& document_url,
    const std::string& document_resource_id)
    : EntryActionOperation(registry, profile, callback, document_url),
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
    GDataOperationRegistry* registry,
    Profile* profile,
    const InitiateUploadCallback& callback,
    const InitiateUploadParams& params)
    : UrlFetchOperation<InitiateUploadCallback>(registry, profile, callback),
      params_(params),
      initiate_upload_url_(chrome_browser_net::AppendQueryParameter(
          params.resumable_create_media_link,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
}

InitiateUploadOperation::~InitiateUploadOperation() {}

GURL InitiateUploadOperation::GetURL() const {
  return initiate_upload_url_;
}

void InitiateUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code =
      static_cast<GDataErrorCode>(source->GetResponseCode());

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

  if (!callback_.is_null()) {
    relay_proxy_->PostTask(FROM_HERE,
                           base::Bind(callback_, code, GURL(upload_location)));
  }
}

void InitiateUploadOperation::RunCallbackOnAuthFailed(GDataErrorCode code) {
  if (!callback_.is_null()) {
    relay_proxy_->PostTask(FROM_HERE,
                           base::Bind(callback_, code, GURL()));
  }
}

URLFetcher::RequestType InitiateUploadOperation::GetRequestType() const {
  return URLFetcher::POST;
}

std::vector<std::string>
InitiateUploadOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kUploadContentType + params_.content_type);
  headers.push_back(
      kUploadContentLength + base::Int64ToString(params_.content_length));
  return headers;
}

bool InitiateUploadOperation::GetContentData(std::string* upload_content_type,
                                             std::string* upload_content) {
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
    GDataOperationRegistry* registry,
    Profile* profile,
    const ResumeUploadCallback& callback,
    const ResumeUploadParams& params)
    : UrlFetchOperation<ResumeUploadCallback>(registry, profile, callback),
      params_(params) {
}

ResumeUploadOperation::~ResumeUploadOperation() {}

GURL ResumeUploadOperation::GetURL() const {
  return params_.upload_location;
}

void ResumeUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());
  net::HttpResponseHeaders* hdrs = source->GetResponseHeaders();
  int64 start_range_received = -1;
  int64 end_range_received = -1;
  std::string resource_id;
  std::string md5_checksum;

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
    DVLOG(1) << "Got response for [" << params_.title
            << "]: code=" << code
            << ", range_hdr=[" << range_received
            << "], range_parsed=" << start_range_received
            << "," << end_range_received;
  } else {
    // There might be explanation of unexpected error code in response.
    std::string response_content;
    source->GetResponseAsString(&response_content);
    DVLOG(1) << "Got response for [" << params_.title
            << "]: code=" << code
            << ", content=[\n" << response_content << "\n]";
    util::ParseCreatedResponseContent(response_content, &resource_id,
                                      &md5_checksum);
    DCHECK(!resource_id.empty());
    DCHECK(!md5_checksum.empty());
  }

  if (!callback_.is_null()) {
    relay_proxy_->PostTask(
        FROM_HERE,
        base::Bind(callback_, ResumeUploadResponse(code, start_range_received,
                   end_range_received, resource_id, md5_checksum)));
  }
}

void ResumeUploadOperation::RunCallbackOnAuthFailed(GDataErrorCode code) {
  if (!callback_.is_null()) {
    relay_proxy_->PostTask(FROM_HERE, base::Bind(callback_,
        ResumeUploadResponse(code, 0, 0, "", "")));
  }
}

URLFetcher::RequestType ResumeUploadOperation::GetRequestType() const {
  return URLFetcher::PUT;
}

std::vector<std::string> ResumeUploadOperation::GetExtraRequestHeaders() const {
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

}  // namespace gdata
