// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

// TODO(kuan): these includes are only used for testing uploading of file to
// gdata.
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace gdata {

namespace {

// All gdata api calls will be initated and processed from this thread.
// TODO(zelidrag): We might want to change this to its own thread
const BrowserThread::ID kGDataAPICallThread = BrowserThread::UI;

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// etag matching header.
const char kIfMatchHeaderFormat[] = "If-Match: %s";

// URL requesting documents list.
const char kGetDocumentListURL[] =
    "https://docs.google.com/feeds/default/private/full?"
    "v=3&alt=json&showfolders=true&max-results=%d";

#ifndef NDEBUG
// Use super small 'page' size while debugging to ensure we hit feed reload
// almost always.
const int kMaxDocumentsPerFeed = 10;
#else
const int kMaxDocumentsPerFeed = 1000;
#endif

const char kFeedField[] = "feed";

// Templates for file uploading.
const char kUploadParamConvertKey[] = "convert";
const char kUploadParamConvertValue[] = "false";
const char kUploadContentType[] = "X-Upload-Content-Type: %s";
const char kUploadContentLength[] = "X-Upload-Content-Length: %u";
const char kUploadContentRangeFormat[] = "Content-Range: bytes %lld-%lld/%u";
const char kUploadResponseLocation[] = "location";
const char kUploadResponseRange[] = "range";

// OAuth scope for the documents API.
const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";

const char* GetExportFormatParam(DocumentExportFormat format) {
  switch (format) {
    case PNG:
      return "png";
    case HTML:
      return "html";
    case TXT:
      return "txt";
    case DOC:
      return "doc";
    case ODT:
      return "odt";
    case RTF:
      return "rtf";
    case ZIP:
      return "zip";
    case JPEG:
      return "jpeg";
    case SVG:
      return "svg";
    case PPT:
      return "ppt";
    case XLS:
      return "xls";
    case CSV:
      return "csv";
    case ODS:
      return "ods";
    case TSV:
      return "tsv";
    default:
      return "pdf";
  }
}

std::string GetResponseHeadersAsString(const content::URLFetcher* url_fetcher) {
  // net::HttpResponseHeaders::raw_headers(), as the name implies, stores
  // all headers in their raw format, i.e each header is null-terminated.
  // So logging raw_headers() only shows the first header, which is probably
  // the status line.  GetNormalizedHeaders, on the other hand, will show all
  // the headers, one per line, which is probably what we want.
  std::string headers;
  // Check that response code indicates response headers are valid (i.e. not
  // malformed) before we retrieve the headers.
  if (url_fetcher->GetResponseCode() ==
      content::URLFetcher::RESPONSE_CODE_INVALID) {
    headers.assign("Response headers are malformed!!");
  } else {
    url_fetcher->GetResponseHeaders()->GetNormalizedHeaders(&headers);
  }
  return headers;
}

}  // namespace

//=============================== UploadFileInfo ===============================

UploadFileInfo::UploadFileInfo()
    : content_length(0),
      file_stream(NULL),
      buf_len(0),
      start_range(0),
      end_range(0) {
}

UploadFileInfo::~UploadFileInfo() {
}

//================================ AuthOperation ===============================

// OAuth2 authorization token retrieval operation.
class AuthOperation : public OAuth2AccessTokenConsumer {
 public:
  AuthOperation(Profile* profile,
                const std::string& refresh_token);
  virtual ~AuthOperation() {}
  void Start(AuthStatusCallback callback);

  // Overriden from OAuth2AccessTokenConsumer:
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

 private:
  Profile* profile_;
  std::string token_;
  AuthStatusCallback callback_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AuthOperation);
};

AuthOperation::AuthOperation(Profile* profile,
                             const std::string& refresh_token)
    : profile_(profile), token_(refresh_token) {
}

void AuthOperation::Start(AuthStatusCallback callback) {
  DCHECK(!token_.empty());
  callback_ = callback;
  std::vector<std::string> scopes;
  scopes.push_back(kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, profile_->GetRequestContext()));
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      token_,
      scopes);
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void AuthOperation::OnGetTokenSuccess(const std::string& access_token) {
  callback_.Run(HTTP_SUCCESS, access_token);
  delete this;
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  LOG(WARNING) << "GDataService: token request using refresh token failed";
  callback_.Run(HTTP_UNAUTHORIZED, std::string());
  delete this;
}

//============================== UrlFetchOperation =============================

// Base class for operation that are fetching URLs.
template <typename T>
class UrlFetchOperation : public content::URLFetcherDelegate {
 public:
  UrlFetchOperation(Profile* profile, const std::string& auth_token)
      : profile_(profile), auth_token_(auth_token), save_temp_file_(false) {
  }

  void Start(T callback) {
    DCHECK(!auth_token_.empty());
    callback_ = callback;
    GURL url = GetURL();
    url_fetcher_.reset(content::URLFetcher::Create(
        url, GetRequestType(), this));
    url_fetcher_->SetRequestContext(profile_->GetRequestContext());
    // Always set flags to neither send nor save cookies.
    url_fetcher_->SetLoadFlags(
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
    if (save_temp_file_) {
      url_fetcher_->SaveResponseToTemporaryFile(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    }

    // Add request headers.
    // Note that SetExtraRequestHeaders clears the current headers and sets it
    // to the passed-in headers, so calling it for each header will result in
    // only the last header being set in request headers.
    url_fetcher_->AddExtraRequestHeader(kGDataVersionHeader);
    url_fetcher_->AddExtraRequestHeader(
          base::StringPrintf(kAuthorizationHeaderFormat, auth_token_.data()));
    std::vector<std::string> headers = GetExtraRequestHeaders();
    for (std::vector<std::string>::iterator iter = headers.begin();
         iter != headers.end(); ++iter) {
      url_fetcher_->AddExtraRequestHeader(*iter);
    }

    // Set upload data if available.
    std::string upload_content_type;
    std::string upload_content;
    if (GetUploadData(&upload_content_type, &upload_content)) {
      url_fetcher_->SetUploadData(upload_content_type, upload_content);
    }

    url_fetcher_->Start();
  }

 protected:
  virtual ~UrlFetchOperation() {}
  // Gets URL for GET request.
  virtual GURL GetURL() const = 0;
  virtual content::URLFetcher::RequestType GetRequestType() const {
    return content::URLFetcher::GET;
  }
  virtual std::vector<std::string> GetExtraRequestHeaders() const {
    return std::vector<std::string>();
  }
  virtual bool GetUploadData(std::string* upload_content_type,
                             std::string* upload_content) {
    return false;
  }

  T callback_;
  Profile* profile_;
  std::string auth_token_;
  bool save_temp_file_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
};

//============================= EntryActionOperation ===========================

// This class performs simple action over a given entry (document/file).
// It is meant to be used for operations that return no JSON blobs.
class EntryActionOperation : public UrlFetchOperation<EntryActionCallback> {
 public:
  EntryActionOperation(Profile* profile,
                       const std::string& auth_token,
                       const GURL& document_url)
      : UrlFetchOperation<EntryActionCallback>(profile,
                                               auth_token),
        document_url_(document_url) {
  }

 protected:
  virtual ~EntryActionOperation() {}

  virtual GURL GetURL() const OVERRIDE { return document_url_; }

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

    if (!callback_.is_null())
      callback_.Run(code, document_url_);

    delete this;
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//=============================== GetDataOperation =============================

// Operation for fetching and parsing JSON data content.
class GetDataOperation : public UrlFetchOperation<GetDataCallback> {
 public:
  GetDataOperation(Profile* profile, const std::string& auth_token)
      : UrlFetchOperation<GetDataCallback>(profile, auth_token) {
  }

 protected:
  virtual ~GetDataOperation() {}

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    std::string data;
    source->GetResponseAsString(&data);
    scoped_ptr<base::Value> root_value;
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

    switch (code) {
      case HTTP_SUCCESS: {
        root_value.reset(ParseResponse(data));
        if (!root_value.get())
          code = GDATA_PARSE_ERROR;

        break;
      }
      default:
        break;
    }

    if (!callback_.is_null())
      callback_.Run(code, root_value.release());

    delete this;
  }

  // Parse GData JSON response.
  static base::Value* ParseResponse(const std::string& data) {
    int error_code = -1;
    std::string error_message;
    scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
        data, false, &error_code, &error_message));
    if (!root_value.get()) {
      LOG(ERROR) << "Error while parsing entry response: "
                 << error_message
                 << ", code: "
                 << error_code;
      return NULL;
    }
    return root_value.release();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

//============================ GetDocumentsOperation ===========================

// Document list fetching operation.
class GetDocumentsOperation : public GetDataOperation  {
 public:
  GetDocumentsOperation(Profile* profile, const std::string& auth_token);
  virtual ~GetDocumentsOperation() {}
  // Sets |url| for document fetching operation. This URL should be set in use
  // case when additional 'pages' of document lists are being fetched.
  void SetUrl(const GURL& url);

 private:
  // Overrides from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

  GURL override_url_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
};

GetDocumentsOperation::GetDocumentsOperation(Profile* profile,
                                             const std::string& auth_token)
    : GetDataOperation(profile, auth_token) {
}

void GetDocumentsOperation::SetUrl(const GURL& url) {
  override_url_ = url;
}

GURL GetDocumentsOperation::GetURL() const {
  if (!override_url_.is_empty())
    return override_url_;

  return GURL(base::StringPrintf(kGetDocumentListURL, kMaxDocumentsPerFeed));
}

//============================ DownloadFileOperation ===========================

// This class performs download of a given entry (document/file).
class DownloadFileOperation : public UrlFetchOperation<DownloadActionCallback> {
 public:
  DownloadFileOperation(Profile* profile,
                        const std::string& auth_token,
                        const GURL& document_url)
      : UrlFetchOperation<DownloadActionCallback>(profile, auth_token),
        document_url_(document_url) {
    // Make sure we download the content into a temp file.
    save_temp_file_ = true;
  }

 protected:
  virtual ~DownloadFileOperation() {}

  virtual GURL GetURL() const OVERRIDE { return document_url_; }

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

    // Take over the ownership of the the downloaded temp file.
    FilePath temp_file;
    if (code == HTTP_SUCCESS &&
        !source->GetResponseAsFilePath(true,  // take_ownership
                                       &temp_file)) {
      code = GDATA_FILE_ERROR;
    }

    if (!callback_.is_null())
      callback_.Run(code, document_url_, temp_file);

    delete this;
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// Document list fetching operation.
class DeleteDocumentOperation : public EntryActionOperation  {
 public:
  DeleteDocumentOperation(Profile* profile,
                          const std::string& auth_token,
                          const GURL& document_url);
  virtual ~DeleteDocumentOperation() {}

 private:
  // Overrides from EntryActionOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
};

DeleteDocumentOperation::DeleteDocumentOperation(Profile* profile,
                                                 const std::string& auth_token,
                                                 const GURL& document_url)
    : EntryActionOperation(profile, auth_token, document_url) {
}

content::URLFetcher::RequestType
DeleteDocumentOperation::GetRequestType() const {
  return content::URLFetcher::DELETE_REQUEST;
}

//=========================== InitiateUploadOperation ==========================

class InitiateUploadOperation
    : public UrlFetchOperation<InitiateUploadCallback> {
 public:
  InitiateUploadOperation(Profile* profile,
                          const std::string& auth_token,
                          const UploadFileInfo& upload_file_info,
                          const GURL& initiate_upload_url);

 protected:
  virtual ~InitiateUploadOperation() {}

  // Overrides from UrlFetcherOperation.
  virtual GURL GetURL() const OVERRIDE;

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  // Overrides from UrlFetcherOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  virtual bool GetUploadData(std::string* upload_content_type,
                             std::string* upload_content) OVERRIDE;

  UploadFileInfo upload_file_info_;
  GURL initiate_upload_url_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperation);
};

InitiateUploadOperation::InitiateUploadOperation(
    Profile* profile,
    const std::string& auth_token,
    const UploadFileInfo& upload_file_info,
    const GURL& initiate_upload_url)
    : UrlFetchOperation<InitiateUploadCallback>(profile, auth_token),
      upload_file_info_(upload_file_info),
      initiate_upload_url_(chrome_browser_net::AppendQueryParameter(
          initiate_upload_url,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
}

GURL InitiateUploadOperation::GetURL() const {
  return initiate_upload_url_;
}

void InitiateUploadOperation::OnURLFetchComplete(
    const content::URLFetcher* source) {
  GDataErrorCode code =
      static_cast<GDataErrorCode>(source->GetResponseCode());
  VLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

  std::string upload_location;
  if (code == HTTP_SUCCESS) {
    // Retrieve value of the first "Location" header.
    source->GetResponseHeaders()->EnumerateHeader(NULL,
                                                  kUploadResponseLocation,
                                                  &upload_location);
  }
  VLOG(1) << "Got response for [" << upload_file_info_.title
          << "]: code=" << code
          << ", location=[" << upload_location << "]";

  if (!callback_.is_null()) {
    callback_.Run(code,
                  upload_file_info_,
                  GURL(upload_location));
  }

  delete this;
}

content::URLFetcher::RequestType
    InitiateUploadOperation::GetRequestType() const {
  return content::URLFetcher::POST;
}

std::vector<std::string>
    InitiateUploadOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(base::StringPrintf(
      kUploadContentType, upload_file_info_.content_type.data()));
  headers.push_back(base::StringPrintf(
      kUploadContentLength, upload_file_info_.content_length));
  return headers;
}

bool InitiateUploadOperation::GetUploadData(std::string* upload_content_type,
                                            std::string* upload_content) {
  upload_content_type->assign("application/atom+xml");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("entry");
  xml_writer.AddAttribute("xmlns", "http://www.w3.org/2005/Atom");
  xml_writer.AddAttribute("xmlns:docs",
                          "http://schemas.google.com/docs/2007");
  xml_writer.WriteElement("title", upload_file_info_.title);
  xml_writer.EndElement(); // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "Upload data: " << *upload_content_type << ", ["
          << *upload_content << "]";
  return true;
}

//============================ ResumeUploadOperation ===========================

class ResumeUploadOperation
    : public UrlFetchOperation<ResumeUploadCallback> {
 public:
  ResumeUploadOperation(Profile* profile,
                        const std::string& auth_token,
                        const UploadFileInfo& upload_file_info);

 protected:
  virtual ~ResumeUploadOperation() {}

  virtual GURL GetURL() const OVERRIDE;

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  // Overrides from UrlFetcherOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  virtual bool GetUploadData(std::string* upload_content_type,
                             std::string* upload_content) OVERRIDE;

  UploadFileInfo upload_file_info_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

ResumeUploadOperation::ResumeUploadOperation(
    Profile* profile,
    const std::string& auth_token,
    const UploadFileInfo& upload_file_info)
    : UrlFetchOperation<ResumeUploadCallback>(profile, auth_token),
      upload_file_info_(upload_file_info) {
}

GURL ResumeUploadOperation::GetURL() const {
  return upload_file_info_.upload_location;
}

void ResumeUploadOperation::OnURLFetchComplete(
    const content::URLFetcher* source) {
  GDataErrorCode code =
      static_cast<GDataErrorCode>(source->GetResponseCode());
  net::HttpResponseHeaders* hdrs = source->GetResponseHeaders();
  std::string range_received;
  std::string response_content;
  int64 start_range_received = -1;
  int64 end_range_received = -1;

  VLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

  if (code == HTTP_RESUME_INCOMPLETE) {
    // Retrieve value of the first "Range" header.
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
    VLOG(1) << "Got response for [" << upload_file_info_.title
            << "]: code=" << code
            << ", range_hdr=[" << range_received
            << "], range_parsed=" << start_range_received
            << "," << end_range_received;
  } else {
    // There might be explanation of unexpected error code in response.
    source->GetResponseAsString(&response_content);
    VLOG(1) << "Got response for [" << upload_file_info_.title
            << "]: code=" << code
            << ", content=[\n" << response_content << "\n]";
  }

  if (!callback_.is_null()) {
    callback_.Run(code, upload_file_info_,
                  start_range_received, end_range_received);
  }

  delete this;
}

content::URLFetcher::RequestType ResumeUploadOperation::GetRequestType() const {
  return content::URLFetcher::PUT;
}

std::vector<std::string> ResumeUploadOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(base::StringPrintf(kUploadContentRangeFormat,
                                       upload_file_info_.start_range,
                                       upload_file_info_.end_range,
                                       upload_file_info_.content_length));
  return headers;
}

bool ResumeUploadOperation::GetUploadData(std::string* upload_content_type,
                                          std::string* upload_content) {
  upload_content_type->assign(upload_file_info_.content_type);
  upload_content->assign(upload_file_info_.buf->data(),
                         upload_file_info_.end_range -
                         upload_file_info_.start_range + 1);
  DVLOG(1) << "Upload data: type=" << *upload_content_type
           << ", range=" << upload_file_info_.start_range
           << "," << upload_file_info_.end_range;
  return true;
}

//================================ GDataService ================================

void GDataService::Initialize(Profile* profile) {
  profile_ = profile;
  // Get OAuth2 refresh token (if we have any) and register for its updates.
  TokenService* service = profile->GetTokenService();
  refresh_token_ = service->GetOAuth2LoginRefreshToken();
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(service));

  if (!refresh_token_.empty())
    OnOAuth2RefreshTokenChanged();
}

GDataService::GDataService() : profile_(NULL) {
}

GDataService::~GDataService() {
}

void GDataService::StartAuthentication(AuthStatusCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));
  (new AuthOperation(profile_, GetOAuth2RefreshToken()))->Start(
      base::Bind(&gdata::GDataService::OnAuthCompleted,
                 AsWeakPtr(),
                 callback));
}

void GDataService::OnAuthCompleted(AuthStatusCallback callback,
                                   GDataErrorCode error,
                                   const std::string& auth_token) {
  if (error == HTTP_SUCCESS)
    auth_token_ = auth_token;
  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  if (!callback.is_null())
    callback.Run(error, auth_token);
}

void GDataService::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() != GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    return;

  auth_token_.clear();
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    refresh_token_ =
        profile_->GetTokenService()->GetOAuth2LoginRefreshToken();
  } else {
    refresh_token_.clear();
  }
  OnOAuth2RefreshTokenChanged();
}

//=============================== DocumentsService =============================

// static.
DocumentsService* DocumentsService::GetInstance() {
  return Singleton<DocumentsService>::get();
}

DocumentsService::DocumentsService()
    : get_documents_started_(false) {
}

DocumentsService::~DocumentsService() {
}

void DocumentsService::GetDocuments(GetDataCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));
  if (!IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(base::Bind(&DocumentsService::GetDocumentsOnAuthRefresh,
                                   base::Unretained(this),
                                   callback));
    return;
  }

  get_documents_started_ = true;
  (new GetDocumentsOperation(profile_, oauth2_auth_token()))->Start(
      base::Bind(&DocumentsService::OnGetDocumentsCompleted,
                 base::Unretained(this),
                 callback));
}

void DocumentsService::GetDocumentsOnAuthRefresh(GetDataCallback callback,
                                                 GDataErrorCode error,
                                                 const std::string& token) {
  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      callback.Run(error, NULL);
    return;
  }
  DCHECK(IsPartiallyAuthenticated());
  GetDocuments(callback);
}

void DocumentsService::OnGetDocumentsCompleted(GetDataCallback callback,
                                               GDataErrorCode error,
                                               base::Value* value) {
  switch (error) {
  case HTTP_UNAUTHORIZED:
    DCHECK(!value);
    auth_token_.clear();
    // User authentication might have expired - rerun the request to force
    // auth token refresh.
    GetDocuments(callback);
    return;
  default:
    break;
  }
  scoped_ptr<base::Value> root_value(value);
  if (!callback.is_null())
    callback.Run(error, root_value.release());
}

void DocumentsService::DownloadDocument(const GURL& document_url,
                                        DocumentExportFormat format,
                                        DownloadActionCallback callback) {
  DownloadFile(
      chrome_browser_net::AppendQueryParameter(document_url,
                                               "exportFormat",
                                               GetExportFormatParam(format)),
      callback);
}

void DocumentsService::DownloadFile(const GURL& document_url,
                                    DownloadActionCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));
  if (!IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(
        base::Bind(&DocumentsService::DownloadDocumentOnAuthRefresh,
                   base::Unretained(this),
                   callback,
                   document_url));
    return;
  }
  (new DownloadFileOperation(
      profile_,
      oauth2_auth_token(),
      document_url))->Start(
          base::Bind(&DocumentsService::OnDownloadDocumentCompleted,
          base::Unretained(this),
          callback));
}

void DocumentsService::DownloadDocumentOnAuthRefresh(
    DownloadActionCallback callback,
    const GURL& document_url,
    GDataErrorCode error,
    const std::string& token) {
  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      callback.Run(error, document_url, FilePath());
    return;
  }
  DCHECK(IsPartiallyAuthenticated());
  DownloadFile(document_url, callback);
}

void DocumentsService::OnDownloadDocumentCompleted(
    DownloadActionCallback callback,
    GDataErrorCode error,
    const GURL& document_url,
    const FilePath& file_path) {
  switch (error) {
    case HTTP_UNAUTHORIZED:
      auth_token_.clear();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      DownloadFile(document_url, callback);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    callback.Run(error, document_url, file_path);
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      EntryActionCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));
  if (!IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(
        base::Bind(&DocumentsService::DeleteDocumentOnAuthRefresh,
                   base::Unretained(this),
                   callback,
                   document_url));
    return;
  }
  (new DeleteDocumentOperation(
      profile_,
      oauth2_auth_token(),
      document_url))->Start(
          base::Bind(&DocumentsService::OnDeleteDocumentCompleted,
          base::Unretained(this),
          callback));
}

void DocumentsService::DeleteDocumentOnAuthRefresh(
    EntryActionCallback callback,
    const GURL& document_url,
    GDataErrorCode error,
    const std::string& token) {
  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      callback.Run(error, document_url);
    return;
  }
  DCHECK(IsPartiallyAuthenticated());
  DeleteDocument(document_url, callback);
}

void DocumentsService::OnDeleteDocumentCompleted(
    EntryActionCallback callback,
    GDataErrorCode error,
    const GURL& document_url) {
  switch (error) {
    case HTTP_UNAUTHORIZED:
      auth_token_.clear();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      DeleteDocument(document_url, callback);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    callback.Run(error, document_url);
}

void DocumentsService::InitiateUpload(const UploadFileInfo& upload_file_info,
                                      InitiateUploadCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  // If we don't have doucment feed, queue caller of InitiateUpload.
  if (!feed_value_.get()) {
    // Check if caller already exists in queue (if file_url is the same);
    // if yes, overwrite it with new upload_file_info and callback.
    InitiateUploadCallerQueue::iterator found = std::find_if(
        initiate_upload_callers_.begin(),
        initiate_upload_callers_.end(),
        SameInitiateUploadCaller(upload_file_info.file_url));

    if (found != initiate_upload_callers_.end()) {
      // Caller already exists in queue, overwrite with the new parameters.
      found->callback = callback;
      found->upload_file_info = upload_file_info;
      DVLOG(1) << "No document feed, overwriting queue slot for "
               << "InitiateUpload caller for ["
               << upload_file_info.title << "] ("
               << upload_file_info.file_url.spec() << ")";
    } else {
      // Caller is new, queue it.
      initiate_upload_callers_.push_back(
          InitiateUploadCaller(callback, upload_file_info));
      DVLOG(1) << "No document feed, queuing InitiateUpload caller for ["
               << upload_file_info.title << "] ("
               << upload_file_info.file_url.spec() << ")";
    }

    // Start GetDocuments if it hasn't even started.
    if (!get_documents_started_) {
      GetDocuments(base::Bind(&DocumentsService::UpdateFilelist,
                              base::Unretained(this)));
    }

    // When UpdateFilelist callback is called after document feed is received,
    // the queue of InitiateUpload callers will be handled.
    return;
  }

  // Retrieve link for resumable-create-media-link to initiate upload.
  scoped_ptr<DocumentFeed> feed;
  const Link* resumable_create_media_link = NULL;
  if (feed_value_.get()) {
    base::DictionaryValue* feed_dict = NULL;
    if (static_cast<DictionaryValue*>(feed_value_.get())->
        GetDictionary(kFeedField, &feed_dict)) {
      feed.reset(DocumentFeed::CreateFrom(feed_dict));
      if (feed.get()) {
        resumable_create_media_link =
            feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
      }
    }
  }
  if (!resumable_create_media_link) {
    if (!callback.is_null())
      callback.Run(HTTP_SERVICE_UNAVAILABLE, upload_file_info, GURL());
    return;
  }

  if (!IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(
        base::Bind(&DocumentsService::InitiateUploadOnAuthRefresh,
                   base::Unretained(this),
                   callback,
                   upload_file_info));
    return;
  }
  (new InitiateUploadOperation(
      profile_,
      oauth2_auth_token(),
      upload_file_info,
      resumable_create_media_link->href()))->Start(
          base::Bind(&DocumentsService::OnInitiateUploadCompleted,
                     base::Unretained(this),
                     callback));
}

void DocumentsService::InitiateUploadOnAuthRefresh(
    InitiateUploadCallback callback,
    const UploadFileInfo& upload_file_info,
    GDataErrorCode error,
    const std::string& token) {
  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      callback.Run(error, upload_file_info, GURL());
    return;
  }
  DCHECK(IsPartiallyAuthenticated());
  InitiateUpload(upload_file_info, callback);
}

void DocumentsService::OnInitiateUploadCompleted(
    InitiateUploadCallback callback,
    GDataErrorCode error,
    const UploadFileInfo& upload_file_info,
    const GURL& upload_location) {
  switch (error) {
    case HTTP_UNAUTHORIZED:
      auth_token_.clear();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      InitiateUpload(upload_file_info, callback);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    callback.Run(error, upload_file_info, upload_location);
}

void DocumentsService::ResumeUpload(const UploadFileInfo& upload_file_info,
                                    ResumeUploadCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  if (!IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(
        base::Bind(&DocumentsService::ResumeUploadOnAuthRefresh,
                   base::Unretained(this),
                   callback,
                   upload_file_info));
    return;
  }

  (new ResumeUploadOperation(
      profile_,
      oauth2_auth_token(),
      upload_file_info))->Start(
          base::Bind(&DocumentsService::OnResumeUploadCompleted,
                     base::Unretained(this),
                     callback));
}

void DocumentsService::ResumeUploadOnAuthRefresh(
    ResumeUploadCallback callback,
    const UploadFileInfo& upload_file_info,
    GDataErrorCode error,
    const std::string& token) {
  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      callback.Run(error, upload_file_info, 0, 0);
    return;
  }
  DCHECK(IsPartiallyAuthenticated());
  ResumeUpload(upload_file_info, callback);
}

void DocumentsService::OnResumeUploadCompleted(
    ResumeUploadCallback callback,
    GDataErrorCode error,
    const UploadFileInfo& upload_file_info,
    int64 start_range_received,
    int64 end_range_received) {
  switch (error) {
    case HTTP_UNAUTHORIZED:
      auth_token_.clear();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      ResumeUpload(upload_file_info, callback);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    callback.Run(error, upload_file_info,
                 start_range_received, end_range_received);
}

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  // TODO(zelidrag): Remove this block once we properly wire these API calls
  // through extension API.
#if defined(TEST_API)
  if (!IsPartiallyAuthenticated())
    return;

  // TODO(zelidrag): Remove this becasue we probably don't want to fetch this
  // before it is really needed.
  if (!feed_value_.get() && !get_documents_started_) {
    GetDocuments(base::Bind(&DocumentsService::UpdateFilelist,
                            base::Unretained(this)));

    // To test file uploading to Google Docs, enable this block and modify the
    // UploadFileInfo structure in the function.
    TestUpload();
  }
#endif
}

void DocumentsService::UpdateFilelist(GDataErrorCode status,
                                      base::Value* data) {
  get_documents_started_ = false;

  if (!(status == HTTP_SUCCESS && data &&
        data->GetType() == Value::TYPE_DICTIONARY))
    return;

  base::DictionaryValue* feed_dict = NULL;
  scoped_ptr<DocumentFeed> feed;
  if (static_cast<DictionaryValue*>(data)->GetDictionary(kFeedField,
                                                         &feed_dict)) {
    feed.reset(DocumentFeed::CreateFrom(feed_dict));
  }
  feed_value_.reset(data);

  // TODO(zelidrag): This part should be removed once we start handling the
  // results properly.
  std::string json;
  base::JSONWriter::Write(feed_value_.get(), true, &json);
  DVLOG(1) << "Received document feed:\n" << json;

  if (feed.get()) {
    DVLOG(1) << "Parsed feed info:"
             << "\n  title = " << feed->title()
             << "\n  start_index = " << feed->start_index()
             << "\n  items_per_page = " << feed->items_per_page()
             << "\n  num_entries = " << feed->entries().size();

    // Now that we have the document feed, start the InitiateUpload operations
    // of queued callers.
    for (size_t i = 0; i < initiate_upload_callers_.size(); ++i) {
      const InitiateUploadCaller& caller = initiate_upload_callers_[i];
      DVLOG(1) << "Starting InitiateUpload for ["
               << caller.upload_file_info.title << "] ("
               << caller.upload_file_info.file_url.spec() << ")";
      InitiateUpload(caller.upload_file_info, caller.callback);
    }
    initiate_upload_callers_.clear();
  }
}

void DocumentsService::TestUpload() {
  DVLOG(1) << "Testing uploading of file to Google Docs";

  UploadFileInfo upload_file_info;

  // TODO(kuan): File operations may block; real app should call them on the
  // file thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // TODO(YOU_testing_upload): Modify the filename, title and content_type of
  // the physical file to upload.
  upload_file_info.title = "Tech Crunch";
  upload_file_info.content_type = "application/pdf";
  FilePath file_path;
  PathService::Get(chrome::DIR_TEST_DATA, &file_path);
  file_path = file_path.AppendASCII("pyauto_private/pdf/TechCrunch.pdf");
  upload_file_info.file_url = GURL(std::string("file:") + file_path.value());

  // Create a FileStream to make sure the file can be opened successfully.
  scoped_ptr<net::FileStream> file(new net::FileStream(NULL));
  int rv = file->Open(file_path,
                      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != net::OK) {
    // If the file can't be opened, we'll just upload an empty file.
    DLOG(WARNING) << "Error opening \"" << file_path.value()
                  << "\" for reading: " << strerror(rv);
    return;
  }
  // Cache the file stream to be used for actual reading of file later.
  upload_file_info.file_stream = file.release();

  // Obtain the file size for content length.
  int64 file_size;
  if (!file_util::GetFileSize(file_path, &file_size)) {
    DLOG(WARNING) << "Error getting file size for " << file_path.value();
    return;
  }

  upload_file_info.content_length = file_size;

  VLOG(1) << "Uploading file: title=[" << upload_file_info.title
          << "], file_url=[" << upload_file_info.file_url.spec()
          << "], content_type=" << upload_file_info.content_type
          << ", content_length=" << upload_file_info.content_length;

  InitiateUpload(upload_file_info,
                 base::Bind(&DocumentsService::OnTestUploadLocationReceived,
                            base::Unretained(this)));
}

void DocumentsService::TestPrepareUploadContent(
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received,
    UploadFileInfo* upload_file_info) {
  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_range), proceed to upload
  // the next chunk.
  if (!(code == HTTP_RESUME_INCOMPLETE &&
        start_range_received == 0 &&
        end_range_received == upload_file_info->end_range)) {
    // TODO(kuan): Real app needs to handle error cases, e.g.
    // - when previously uploaded data wasn't received by Google Docs server,
    //   i.e. when end_range_received < upload_file_info->end_range
    // - when quota is exceeded, which is 1GB for files not converted to Google
    //   Docs format; even though the quota-exceeded content length (I tried
    //   1.91GB) is specified in the header when posting request to get upload
    //   location, the server allows us to upload all chunks of entire file
    //   successfully, but instead of returning 201 (CREATED) status code after
    //   receiving the last chunk, it returns 403 (FORBIDDEN); response content
    //   then will indicate quote exceeded exception.
    // File operations may block; real app should call them on the file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info->file_stream;
    return;
  }

  // If end_range_received is 0, we're at start of uploading process.
  bool first_upload = end_range_received == 0;

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  int64 size_remaining = upload_file_info->content_length;
  if (!first_upload)
    size_remaining -= end_range_received + 1;
  int bytes_to_read = std::min(size_remaining,
                               static_cast<int64>(upload_file_info->buf_len));

  // TODO(kuan): Read file operation could be blocking, real app should do it
  // on the file thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  int bytes_read = upload_file_info->file_stream->Read(
      upload_file_info->buf->data(),
      bytes_to_read,
      net::CompletionCallback());
  if (bytes_read <= 0) {
    DVLOG(1) << "Error reading from file "
             << upload_file_info->file_url.spec();
    memset(upload_file_info->buf->data(), 0, bytes_to_read);
    bytes_read = bytes_to_read;
  }

  // Update |upload_file_info| with ranges to upload.
  upload_file_info->start_range = first_upload ? 0 : end_range_received + 1;
  upload_file_info->end_range = upload_file_info->start_range +
                                bytes_read - 1;

  ResumeUpload(*upload_file_info,
               base::Bind(&DocumentsService::OnTestResumeUploadResponseReceived,
                          base::Unretained(this)));
}

void DocumentsService::OnTestUploadLocationReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    const GURL& upload_location) {
  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  UploadFileInfo upload_file_info = in_upload_file_info;

  if (code != HTTP_SUCCESS) {
    // TODO(kuan): Real app should handle error codes from Google Docs server.
    // File operations may block; real app should call them on the file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info.file_stream;
    return;
  }

  upload_file_info.upload_location = upload_location;

  // Create buffer to hold upload data.
  // Google Documents List API requires uploading chunk size of 512 kilobytes.
  const size_t kBufSize = 524288;
  upload_file_info.buf_len = std::min(upload_file_info.content_length,
                                      kBufSize);
  upload_file_info.buf = new net::IOBuffer(upload_file_info.buf_len);

  // Change code to RESUME_INCOMPLETE and set range to 0-0, so that
  // PrepareUploadContent will start from beginning.
  TestPrepareUploadContent(HTTP_RESUME_INCOMPLETE, 0, 0, &upload_file_info);
}

void DocumentsService::OnTestResumeUploadResponseReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    int64 start_range_received,
    int64 end_range_received) {
  UploadFileInfo upload_file_info = in_upload_file_info;

  if (code == HTTP_CREATED) {
    DVLOG(1) << "Successfully created uploaded file=["
             << in_upload_file_info.title << "] ("
             << in_upload_file_info.file_url.spec() << ")";
    // We're done with uploading, so file stream is no longer needed.
    // TODO(kuan): File operations may block; real app should call them on the
    // file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info.file_stream;
    return;
  }

  DVLOG(1) << "Got received range " << start_range_received
           << "-" << end_range_received
           << " for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  // Continue uploading if necessary.
  TestPrepareUploadContent(code, start_range_received, end_range_received,
                           &upload_file_info);
}

}  // namespace gdata
