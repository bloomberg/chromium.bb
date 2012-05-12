// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operations.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/url_util.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"

using content::BrowserThread;
using content::URLFetcher;

namespace {

// Used for success ratio histograms. 0 for failure, 1 for success,
// 2 for no connection (likely offline).
const int kSuccessRatioHistogramFailure = 0;
const int kSuccessRatioHistogramSuccess = 1;
const int kSuccessRatioHistogramNoConnection = 2;
const int kSuccessRatioHistogramMaxValue = 3;  // The max value is exclusive.

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// etag matching header.
const char kIfMatchAllHeader[] = "If-Match: *";
const char kIfMatchHeaderFormat[] = "If-Match: %s";

// URL requesting documents list that belong to the authenticated user only
// (handled with '-/mine' part).
const char kGetDocumentListURL[] =
    "https://docs.google.com/feeds/default/private/full/-/mine";

// URL requesting documents list of changes to documents collections.
const char kGetChangesListURL[] =
    "https://docs.google.com/feeds/default/private/changes";

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

// Maximum number of attempts for re-authentication per operation.
const int kMaxReAuthenticateAttemptsPerOperation = 1;

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
      this, g_browser_process->system_request_context()));
  NotifyStart();
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      token_,
      scopes);
}

void AuthOperation::DoCancel() {
  oauth2_access_token_fetcher_->CancelRequest();
  if (!callback_.is_null())
    callback_.Run(GDATA_CANCELLED, std::string());
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void AuthOperation::OnGetTokenSuccess(const std::string& access_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                            kSuccessRatioHistogramSuccess,
                            kSuccessRatioHistogramMaxValue);

  callback_.Run(HTTP_SUCCESS, access_token);
  NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LOG(WARNING) << "AuthOperation: token request using refresh token failed"
               << error.ToString();

  // There are many ways to fail, but if the failure is due to connection,
  // it's likely that the device is off-line. We treat the error differently
  // so that the file manager works while off-line.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                              kSuccessRatioHistogramNoConnection,
                              kSuccessRatioHistogramMaxValue);
    callback_.Run(GDATA_NO_CONNECTION, std::string());
  } else {
    UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                              kSuccessRatioHistogramFailure,
                              kSuccessRatioHistogramMaxValue);
    callback_.Run(HTTP_UNAUTHORIZED, std::string());
  }
  NotifyFinish(GDataOperationRegistry::OPERATION_FAILED);
}

//============================ UrlFetchOperationBase ===========================

UrlFetchOperationBase::UrlFetchOperationBase(GDataOperationRegistry* registry,
                                             Profile* profile)
    : GDataOperationRegistry::Operation(registry),
      profile_(profile),
      re_authenticate_count_(0),
      save_temp_file_(false),
      started_(false) {
}

UrlFetchOperationBase::UrlFetchOperationBase(
    GDataOperationRegistry* registry,
    GDataOperationRegistry::OperationType type,
    const FilePath& path,
    Profile* profile)
    : GDataOperationRegistry::Operation(registry, type, path),
      profile_(profile),
      re_authenticate_count_(0),
      save_temp_file_(false) {
}

UrlFetchOperationBase::~UrlFetchOperationBase() {}

void UrlFetchOperationBase::Start(const std::string& auth_token) {
  DCHECK(!auth_token.empty());

  GURL url = GetURL();
  DCHECK(!url.is_empty());
  DVLOG(1) << "URL: " << url.spec();

  url_fetcher_.reset(URLFetcher::Create(url, GetRequestType(), this));
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  // Always set flags to neither send nor save cookies.
  url_fetcher_->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DISABLE_CACHE);
  if (save_temp_file_) {
    url_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  } else if (!output_file_path_.empty()) {
    url_fetcher_->SaveResponseToFileAtPath(output_file_path_,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }

  // Add request headers.
  // Note that SetExtraRequestHeaders clears the current headers and sets it
  // to the passed-in headers, so calling it for each header will result in
  // only the last header being set in request headers.
  url_fetcher_->AddExtraRequestHeader(kGDataVersionHeader);
  url_fetcher_->AddExtraRequestHeader(
        base::StringPrintf(kAuthorizationHeaderFormat, auth_token.data()));
  std::vector<std::string> headers = GetExtraRequestHeaders();
  for (size_t i = 0; i < headers.size(); ++i) {
    url_fetcher_->AddExtraRequestHeader(headers[i]);
    DVLOG(1) << "Extra header: " << headers[i];
  }

  // Set upload data if available.
  std::string upload_content_type;
  std::string upload_content;
  if (GetContentData(&upload_content_type, &upload_content)) {
    url_fetcher_->SetUploadData(upload_content_type, upload_content);
  }

  // Register to operation registry.
  NotifyStartToOperationRegistry();

  url_fetcher_->Start();
  started_ = true;
}

void UrlFetchOperationBase::SetReAuthenticateCallback(
    const ReAuthenticateCallback& callback) {
  DCHECK(re_authenticate_callback_.is_null());

  re_authenticate_callback_ = callback;
}

URLFetcher::RequestType UrlFetchOperationBase::GetRequestType() const {
  return URLFetcher::GET;
}

std::vector<std::string> UrlFetchOperationBase::GetExtraRequestHeaders() const {
  return std::vector<std::string>();
}

bool UrlFetchOperationBase::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  return false;
}

void UrlFetchOperationBase::DoCancel() {
  url_fetcher_.reset(NULL);
  RunCallbackOnPrematureFailure(GDATA_CANCELLED);
}

void UrlFetchOperationBase::OnURLFetchComplete(const net::URLFetcher* source) {
  GDataErrorCode code =
      static_cast<GDataErrorCode>(source->GetResponseCode());
  DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

  if (code == HTTP_UNAUTHORIZED) {
    if (!re_authenticate_callback_.is_null() &&
        ++re_authenticate_count_ <= kMaxReAuthenticateAttemptsPerOperation) {
      re_authenticate_callback_.Run(this);
      return;
    }

    OnAuthFailed(code);
    return;
  }

  // Overridden by each specialization
  bool success = ProcessURLFetchResults(source);
  if (success)
    NotifySuccessToOperationRegistry();
  else
    NotifyFinish(GDataOperationRegistry::OPERATION_FAILED);
}

void UrlFetchOperationBase::NotifySuccessToOperationRegistry() {
  NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
}

void UrlFetchOperationBase::NotifyStartToOperationRegistry() {
  NotifyStart();
}

void UrlFetchOperationBase::OnAuthFailed(GDataErrorCode code) {
  RunCallbackOnPrematureFailure(code);
  // Check if this failed before we even started fetching. If so, register
  // for start so we can properly unregister with finish.
  if (!started_)
    NotifyStart();

  NotifyFinish(GDataOperationRegistry::OPERATION_FAILED);

  // Notify authentication failed.
  NotifyAuthFailed();
}

std::string UrlFetchOperationBase::GetResponseHeadersAsString(
    const net::URLFetcher* url_fetcher) {
  // net::HttpResponseHeaders::raw_headers(), as the name implies, stores
  // all headers in their raw format, i.e each header is null-terminated.
  // So logging raw_headers() only shows the first header, which is probably
  // the status line.  GetNormalizedHeaders, on the other hand, will show all
  // the headers, one per line, which is probably what we want.
  std::string headers;
  // Check that response code indicates response headers are valid (i.e. not
  // malformed) before we retrieve the headers.
  if (url_fetcher->GetResponseCode() == URLFetcher::RESPONSE_CODE_INVALID) {
    headers.assign("Response headers are malformed!!");
  } else {
    url_fetcher->GetResponseHeaders()->GetNormalizedHeaders(&headers);
  }
  return headers;
}

//============================ EntryActionOperation ============================

EntryActionOperation::EntryActionOperation(GDataOperationRegistry* registry,
                                           Profile* profile,
                                           const EntryActionCallback& callback,
                                           const GURL& document_url)
    : UrlFetchOperationBase(registry, profile),
      callback_(callback),
      document_url_(document_url) {
}

EntryActionOperation::~EntryActionOperation() {}

// Overridden from UrlFetchOperationBase.
GURL EntryActionOperation::GetURL() const {
  return AddStandardUrlParams(document_url_);
}

bool EntryActionOperation::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  if (!callback_.is_null()) {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    callback_.Run(code, document_url_);
  }
  return true;
}

void EntryActionOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  if (!callback_.is_null())
    callback_.Run(code, document_url_);
}

//============================== GetDataOperation ==============================

GetDataOperation::GetDataOperation(GDataOperationRegistry* registry,
                                   Profile* profile,
                                   const GetDataCallback& callback)
    : UrlFetchOperationBase(registry, profile), callback_(callback) {
}

GetDataOperation::~GetDataOperation() {}

bool GetDataOperation::ProcessURLFetchResults(const net::URLFetcher* source) {
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

  if (!callback_.is_null())
    callback_.Run(code, root_value.Pass());
  return root_value.get() != NULL;
}

void GetDataOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  if (!callback_.is_null()) {
    scoped_ptr<base::Value> root_value;
    callback_.Run(code, root_value.Pass());
  }
}

// static
base::Value* GetDataOperation::ParseResponse(const std::string& data) {
  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
      data, base::JSON_PARSE_RFC, &error_code, &error_message));
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
                                             int start_changestamp,
                                             const std::string& search_string,
                                             const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback),
      start_changestamp_(start_changestamp),
      search_string_(search_string) {
}

GetDocumentsOperation::~GetDocumentsOperation() {}

void GetDocumentsOperation::SetUrl(const GURL& url) {
  override_url_ = url;
}

GURL GetDocumentsOperation::GetURL() const {
  if (!override_url_.is_empty())
    return AddFeedUrlParams(override_url_,
                            kMaxDocumentsPerFeed,
                            0,
                            std::string());

  if (start_changestamp_ == 0) {
    return AddFeedUrlParams(GURL(kGetDocumentListURL),
                            kMaxDocumentsPerFeed,
                            0,
                            search_string_);
  }

  return AddFeedUrlParams(GURL(kGetChangesListURL),
                          kMaxDocumentsPerFeed,
                          start_changestamp_,
                          std::string());
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
  return AddMetadataUrlParams(GURL(kAccountMetadataURL));
}

//============================ DownloadFileOperation ===========================

DownloadFileOperation::DownloadFileOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const DownloadActionCallback& download_action_callback,
    const GetDownloadDataCallback& get_download_data_callback,
    const GURL& document_url,
    const FilePath& virtual_path,
    const FilePath& output_file_path)
    : UrlFetchOperationBase(registry,
                            GDataOperationRegistry::OPERATION_DOWNLOAD,
                            virtual_path,
                            profile),
      download_action_callback_(download_action_callback),
      get_download_data_callback_(get_download_data_callback),
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

void DownloadFileOperation::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  NotifyProgress(current, total);
}

bool DownloadFileOperation::ShouldSendDownloadData() {
  return !get_download_data_callback_.is_null();
}

void DownloadFileOperation::OnURLFetchDownloadData(
    const net::URLFetcher* source,
    scoped_ptr<std::string> download_data) {
  if (!get_download_data_callback_.is_null())
    get_download_data_callback_.Run(HTTP_SUCCESS, download_data.Pass());
}

bool DownloadFileOperation::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());

  // Take over the ownership of the the downloaded temp file.
  FilePath temp_file;
  if (code == HTTP_SUCCESS &&
      !source->GetResponseAsFilePath(true,  // take_ownership
                                     &temp_file)) {
    code = GDATA_FILE_ERROR;
  }

  if (!download_action_callback_.is_null())
    download_action_callback_.Run(code, document_url_, temp_file);
  return code == HTTP_SUCCESS;
}

void DownloadFileOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  if (!download_action_callback_.is_null())
    download_action_callback_.Run(code, document_url_, FilePath());
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
    const std::string& resource_id,
    const FilePath::StringType& new_name)
    : GetDataOperation(registry, profile, callback),
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
    : UrlFetchOperationBase(registry,
                            GDataOperationRegistry::OPERATION_UPLOAD,
                            params.virtual_path,
                            profile),
      callback_(callback),
      params_(params),
      initiate_upload_url_(chrome_common_net::AppendOrReplaceQueryParameter(
          params.resumable_create_media_link,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
}

InitiateUploadOperation::~InitiateUploadOperation() {}

GURL InitiateUploadOperation::GetURL() const {
  return initiate_upload_url_;
}

bool InitiateUploadOperation::ProcessURLFetchResults(
    const net::URLFetcher* source) {
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

  if (!callback_.is_null())
    callback_.Run(code, GURL(upload_location));
  return code == HTTP_SUCCESS;
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
  return URLFetcher::POST;
}

std::vector<std::string>
InitiateUploadOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  if (!params_.content_type.empty())
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
  : UrlFetchOperationBase(registry,
                          GDataOperationRegistry::OPERATION_UPLOAD,
                          params.virtual_path,
                          profile),
      callback_(callback),
      params_(params),
      last_chunk_completed_(false) {
}

ResumeUploadOperation::~ResumeUploadOperation() {}

GURL ResumeUploadOperation::GetURL() const {
  return params_.upload_location;
}

bool ResumeUploadOperation::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());
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

  if (code == HTTP_CREATED)
    last_chunk_completed_ = true;

  return code == HTTP_CREATED || code == HTTP_RESUME_INCOMPLETE;
}

void ResumeUploadOperation::NotifyStartToOperationRegistry() {
  NotifyResume();
}

void ResumeUploadOperation::NotifySuccessToOperationRegistry() {
  if (last_chunk_completed_)
    NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
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
    const net::URLFetcher* source, int64 current, int64 total) {
  // Adjust the progress values according to the range currently uploaded.
  NotifyProgress(params_.start_range + current, params_.content_length);
}

}  // namespace gdata
