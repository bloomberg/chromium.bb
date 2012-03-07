// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "net/base/file_stream.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using content::BrowserThread;
using content::URLFetcher;
using content::URLFetcherDelegate;

namespace gdata {

namespace {

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// etag matching header.
const char kIfMatchHeaderFormat[] = "If-Match: %s";

// URL requesting documents list that belong to the authenticated user only
// (handled with '-/mine' part).
const char kGetDocumentListURL[] =
    "https://docs.google.com/feeds/default/private/full/-/mine";

// Root document list url.
const char kDocumentListRootURL[] =
    "https://docs.google.com/feeds/default/private/full";

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
const char kIfMatchAllHeader[] = "If-Match: *";

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

std::string GetResponseHeadersAsString(const URLFetcher* url_fetcher) {
  // net::HttpResponseHeaders::raw_headers(), as the name implies, stores
  // all headers in their raw format, i.e each header is null-terminated.
  // So logging raw_headers() only shows the first header, which is probably
  // the status line.  GetNormalizedHeaders, on the other hand, will show all
  // the headers, one per line, which is probably what we want.
  std::string headers;
  // Check that response code indicates response headers are valid (i.e. not
  // malformed) before we retrieve the headers.
  if (url_fetcher->GetResponseCode() ==
      URLFetcher::RESPONSE_CODE_INVALID) {
    headers.assign("Response headers are malformed!!");
  } else {
    url_fetcher->GetResponseHeaders()->GetNormalizedHeaders(&headers);
  }
  return headers;
}

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

//============================== InitiateUploadParams ==========================

InitiateUploadParams::InitiateUploadParams(
    const std::string& title,
    const std::string& content_type,
    int64 content_length,
    const GURL& resumable_create_media_link)
    : title(title),
      content_type(content_type),
      content_length(content_length),
      resumable_create_media_link(resumable_create_media_link) {
}

InitiateUploadParams::~InitiateUploadParams() {
}

//================================ ResumeUploadParams===========================

ResumeUploadParams::ResumeUploadParams(const std::string& title,
    int64 start_range,
    int64 end_range,
    int64 content_length,
    const std::string& content_type,
    scoped_refptr<net::IOBuffer> buf,
    const GURL& upload_location) : title(title),
                                   start_range(start_range),
                                   end_range(end_range),
                                   content_length(content_length),
                                   content_type(content_type),
                                   buf(buf),
                                   upload_location(upload_location) {
}

ResumeUploadParams::~ResumeUploadParams() {
}

//================================ AuthOperation ===============================

// OAuth2 authorization token retrieval operation.
class AuthOperation : public GDataOperationRegistry::Operation,
                      public OAuth2AccessTokenConsumer {
 public:
  AuthOperation(GDataOperationRegistry* registry,
                Profile* profile,
                const std::string& refresh_token);
  virtual ~AuthOperation() {}
  void Start(AuthStatusCallback callback);

  // Overriden from OAuth2AccessTokenConsumer:
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from GDataOpertionRegistry::Operation
  virtual void DoCancel() OVERRIDE;

 private:
  Profile* profile_;
  std::string token_;
  AuthStatusCallback callback_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AuthOperation);
};

AuthOperation::AuthOperation(GDataOperationRegistry* registry,
                             Profile* profile,
                             const std::string& refresh_token)
    : GDataOperationRegistry::Operation(registry),
      profile_(profile), token_(refresh_token) {
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
  callback_.Run(HTTP_SUCCESS, access_token);
  NotifyFinish(OPERATION_SUCCESS);
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  LOG(WARNING) << "AuthOperation: token request using refresh token failed";
  callback_.Run(HTTP_UNAUTHORIZED, std::string());
  NotifyFinish(OPERATION_FAILURE);
}

//============================== UrlFetchOperation =============================

// Base class for operation that are fetching URLs.
template <typename T>
class UrlFetchOperation : public GDataOperationRegistry::Operation,
                          public URLFetcherDelegate {
 public:
  UrlFetchOperation(GDataOperationRegistry* registry,
                    Profile* profile,
                    const std::string& auth_token)
      : GDataOperationRegistry::Operation(registry),
        profile_(profile), auth_token_(auth_token), save_temp_file_(false) {
  }

  void Start(T callback) {
    DCHECK(!auth_token_.empty());
    callback_ = callback;
    GURL url = GetURL();
    url_fetcher_.reset(URLFetcher::Create(
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
    for (size_t i = 0; i < headers.size(); ++i) {
      url_fetcher_->AddExtraRequestHeader(headers[i]);
      DVLOG(1) << "Extra header " << headers[i];
    }

    // Set upload data if available.
    std::string upload_content_type;
    std::string upload_content;
    if (GetContentData(&upload_content_type, &upload_content)) {
      url_fetcher_->SetUploadData(upload_content_type, upload_content);
    }

    // Register to operation registry.
    NotifyStart();

    url_fetcher_->Start();
  }

 protected:
  virtual ~UrlFetchOperation() {}
  // Gets URL for GET request.
  virtual GURL GetURL() const = 0;
  virtual URLFetcher::RequestType GetRequestType() const {
    return URLFetcher::GET;
  }
  virtual std::vector<std::string> GetExtraRequestHeaders() const {
    return std::vector<std::string>();
  }
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) {
    return false;
  }
  virtual void ProcessURLFetchResults(const URLFetcher* source) = 0;

  // Implement GDataOperationRegistry::Operation
  virtual void DoCancel() OVERRIDE {
    url_fetcher_.reset(NULL);
  }

  // Implement URLFetcherDelegate.
  // TODO(kinaba): http://crosbug.com/27370
  // Current URLFetcherDelegate notifies only the progress of "download"
  // transfers, and no notification for upload progress in POST/PUT.
  // For some GData operations, however, progress of uploading transfer makes
  // more sense. We need to add a way to track upload status.
  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE {
    NotifyProgress(current, total);
  }

  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
    // Overriden by each specialization
    ProcessURLFetchResults(source);
    NotifyFinish(OPERATION_SUCCESS);
  }

  T callback_;
  Profile* profile_;
  std::string auth_token_;
  bool save_temp_file_;
  scoped_ptr<URLFetcher> url_fetcher_;
};

//============================= EntryActionOperation ===========================

// This class performs simple action over a given entry (document/file).
// It is meant to be used for operations that return no JSON blobs.
class EntryActionOperation : public UrlFetchOperation<EntryActionCallback> {
 public:
  EntryActionOperation(GDataOperationRegistry* registry,
                       Profile* profile,
                       const std::string& auth_token,
                       const GURL& document_url)
      : UrlFetchOperation<EntryActionCallback>(registry, profile, auth_token),
        document_url_(document_url) {
  }

 protected:
  virtual ~EntryActionOperation() {}

  virtual GURL GetURL() const OVERRIDE {
    return AddStandardUrlParams(document_url_);
  }

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

    if (!callback_.is_null())
      callback_.Run(code, document_url_);
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//=============================== GetDataOperation =============================

// Operation for fetching and parsing JSON data content.
class GetDataOperation : public UrlFetchOperation<GetDataCallback> {
 public:
  GetDataOperation(GDataOperationRegistry* registry,
                   Profile* profile,
                   const std::string& auth_token)
      : UrlFetchOperation<GetDataCallback>(registry, profile, auth_token) {
  }

 protected:
  virtual ~GetDataOperation() {}

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
    std::string data;
    source->GetResponseAsString(&data);
    scoped_ptr<base::Value> root_value;
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

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
      callback_.Run(code, root_value.release());
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
                 << error_code
                 << ", data:\n"
                 << data;
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
  GetDocumentsOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const std::string& auth_token);
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

GetDocumentsOperation::GetDocumentsOperation(GDataOperationRegistry* registry,
                                             Profile* profile,
                                             const std::string& auth_token)
    : GetDataOperation(registry, profile, auth_token) {
}

void GetDocumentsOperation::SetUrl(const GURL& url) {
  override_url_ = url;
}

GURL GetDocumentsOperation::GetURL() const {
  if (!override_url_.is_empty())
    return AddFeedUrlParams(override_url_);

  return AddFeedUrlParams(GURL(kGetDocumentListURL));
}

//============================ DownloadFileOperation ===========================

// This class performs download of a given entry (document/file).
class DownloadFileOperation : public UrlFetchOperation<DownloadActionCallback> {
 public:
  DownloadFileOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const std::string& auth_token,
                        const GURL& document_url)
      : UrlFetchOperation<DownloadActionCallback>(registry,
                                                  profile,
                                                  auth_token),
        document_url_(document_url) {
    // Make sure we download the content into a temp file.
    save_temp_file_ = true;
  }

 protected:
  virtual ~DownloadFileOperation() {}

  virtual GURL GetURL() const OVERRIDE { return document_url_; }

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
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
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// Document list fetching operation.
class DeleteDocumentOperation : public EntryActionOperation  {
 public:
  DeleteDocumentOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const std::string& auth_token,
                          const GURL& document_url);
  virtual ~DeleteDocumentOperation() {}

 private:
  // Overrides from EntryActionOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
};

DeleteDocumentOperation::DeleteDocumentOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const std::string& auth_token,
    const GURL& document_url)
    : EntryActionOperation(registry, profile, auth_token, document_url) {
}

URLFetcher::RequestType
DeleteDocumentOperation::GetRequestType() const {
  return URLFetcher::DELETE_REQUEST;
}

std::vector<std::string>
DeleteDocumentOperation::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kIfMatchAllHeader);
  return headers;
}


//=========================== CreateDirectoryOperation =========================

class CreateDirectoryOperation
    : public GetDataOperation {
 public:
  // Empty |parent_content_url| will create the directory in the the root
  // folder.
  CreateDirectoryOperation(GDataOperationRegistry* registry,
                           Profile* profile,
                           const std::string& auth_token,
                           const GURL& parent_content_url,
                           const FilePath::StringType& directory_name);

 protected:
  virtual ~CreateDirectoryOperation() {}

  // Overrides from UrlFetcherOperation.
  virtual GURL GetURL() const OVERRIDE;

  // URLFetcherDelegate overrides.
  virtual URLFetcher::RequestType GetRequestType() const;

 private:
  // Overrides from UrlFetcherOperation.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  const GURL parent_content_url_;
  const FilePath::StringType directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

CreateDirectoryOperation::CreateDirectoryOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const std::string& auth_token,
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name)
    : GetDataOperation(registry, profile, auth_token),
      parent_content_url_(parent_content_url),
      directory_name_(directory_name) {
}

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

//=========================== InitiateUploadOperation ==========================

class InitiateUploadOperation
    : public UrlFetchOperation<InitiateUploadCallback> {
 public:
  InitiateUploadOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const std::string& auth_token,
                          const InitiateUploadParams& params);

 protected:
  virtual ~InitiateUploadOperation() {}

  // Overrides from UrlFetcherOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE;

 private:
  // Overrides from UrlFetcherOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;

  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  InitiateUploadParams params_;
  GURL initiate_upload_url_;
  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperation);
};

InitiateUploadOperation::InitiateUploadOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const std::string& auth_token,
    const InitiateUploadParams& params)
    : UrlFetchOperation<InitiateUploadCallback>(registry, profile, auth_token),
      params_(params),
      initiate_upload_url_(chrome_browser_net::AppendQueryParameter(
          params.resumable_create_media_link,
          kUploadParamConvertKey,
          kUploadParamConvertValue)) {
}

GURL InitiateUploadOperation::GetURL() const {
  return initiate_upload_url_;
}

void InitiateUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
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
  VLOG(1) << "Got response for [" << params_.title
          << "]: code=" << code
          << ", location=[" << upload_location << "]";

  if (!callback_.is_null()) {
    callback_.Run(code,
                  GURL(upload_location));
  }
}

URLFetcher::RequestType
    InitiateUploadOperation::GetRequestType() const {
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

class ResumeUploadOperation
    : public UrlFetchOperation<ResumeUploadCallback> {
 public:
  ResumeUploadOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const std::string& auth_token,
                        const ResumeUploadParams& params);

 protected:
  virtual ~ResumeUploadOperation() {}

  virtual GURL GetURL() const OVERRIDE;

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE;

 private:
  // Overrides from UrlFetcherOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;

  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  ResumeUploadParams params_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

ResumeUploadOperation::ResumeUploadOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const std::string& auth_token,
    const ResumeUploadParams& params)
    : UrlFetchOperation<ResumeUploadCallback>(registry, profile, auth_token),
      params_(params) {
}

GURL ResumeUploadOperation::GetURL() const {
  return params_.upload_location;
}

void ResumeUploadOperation::ProcessURLFetchResults(const URLFetcher* source) {
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
    VLOG(1) << "Got response for [" << params_.title
            << "]: code=" << code
            << ", range_hdr=[" << range_received
            << "], range_parsed=" << start_range_received
            << "," << end_range_received;
  } else {
    // There might be explanation of unexpected error code in response.
    source->GetResponseAsString(&response_content);
    VLOG(1) << "Got response for [" << params_.title
            << "]: code=" << code
            << ", content=[\n" << response_content << "\n]";
  }

  if (!callback_.is_null()) {
    callback_.Run(code, start_range_received, end_range_received);
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
  *upload_content =
      std::string(params_.buf->data(),
                  params_.end_range - params_.start_range + 1);
  return true;
}

//================================ GDataAuthService ============================

void GDataAuthService::Initialize(Profile* profile) {
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
    FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

GDataAuthService::GDataAuthService()
    : profile_(NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

GDataAuthService::~GDataAuthService() {
}

void GDataAuthService::StartAuthentication(GDataOperationRegistry* registry,
                                           AuthStatusCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  (new AuthOperation(registry, profile_, GetOAuth2RefreshToken()))->Start(
      base::Bind(&gdata::GDataAuthService::OnAuthCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GDataAuthService::OnAuthCompleted(AuthStatusCallback callback,
                                   GDataErrorCode error,
                                   const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == HTTP_SUCCESS)
    auth_token_ = auth_token;
  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  if (!callback.is_null())
    callback.Run(error, auth_token);
}

void GDataAuthService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GDataAuthService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GDataAuthService::Observe(int type,
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
  FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

//=============================== DocumentsService =============================

DocumentsService::DocumentsService()
    : profile_(NULL),
      gdata_auth_service_(new GDataAuthService()),
      operation_registry_(new GDataOperationRegistry),
      weak_ptr_factory_(this) {
  // The weak pointers is used to post tasks to UI thread.
  weak_ptr_bound_to_ui_thread_ = weak_ptr_factory_.GetWeakPtr();
}

DocumentsService::~DocumentsService() {
  gdata_auth_service_->RemoveObserver(this);
}

void DocumentsService::Initialize(Profile* profile) {
  profile_ = profile;
  // AddObserver() should be called before Initialize() as it could change
  // the refresh token.
  gdata_auth_service_->AddObserver(this);
  gdata_auth_service_->Initialize(profile);
}

void DocumentsService::CancelAll() {
  operation_registry_->CancelAll();
}

void DocumentsService::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (gdata_auth_service_->IsFullyAuthenticated()) {
    callback.Run(gdata::HTTP_SUCCESS, gdata_auth_service_->oauth2_auth_token());
  } else if (gdata_auth_service_->IsPartiallyAuthenticated()) {
    // We have refresh token, let's gets authenticated.
    gdata_auth_service_->StartAuthentication(operation_registry_.get(),
                                             callback);
  } else {
    callback.Run(gdata::HTTP_UNAUTHORIZED, std::string());
  }
}

void DocumentsService::GetDocuments(const GURL& url,
                                    const GetDataCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::GetDocumentsOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          url,
          callback,
          // MessageLoopProxy is used to run |callback| on the origin thread.
          base::MessageLoopProxy::current()));
}

void DocumentsService::GetDocumentsOnUIThread(
    const GURL& url,
    const GetDataCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::GetDocumentsOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   url,
                   callback,
                   relay_proxy));
    return;
  }

  // operation's lifetime is managed by operation_registry_, we don't need to
  // clean it here.
  GetDocumentsOperation* operation = new GetDocumentsOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token());
  if (!url.is_empty())
    operation->SetUrl(url);

  operation->Start(base::Bind(&DocumentsService::OnGetDocumentsCompleted,
                              weak_ptr_factory_.GetWeakPtr(),
                              url,
                              callback,
                              relay_proxy));
}

void DocumentsService::GetDocumentsOnAuthRefresh(
    const GURL& url,
    const GetDataCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      relay_proxy->PostTask(
          FROM_HERE,
          base::Bind(callback, error, static_cast<base::Value*>(NULL)));
   return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  GetDocumentsOnUIThread(url, callback, relay_proxy);
}

void DocumentsService::OnGetDocumentsCompleted(
    const GURL& url,
    const GetDataCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    base::Value* value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
  case HTTP_UNAUTHORIZED:
    DCHECK(!value);
    gdata_auth_service_->ClearOAuth2Token();
    // User authentication might have expired - rerun the request to force
    // auth token refresh.
    GetDocumentsOnUIThread(url, callback, relay_proxy);
    return;
  default:
    break;
  }
  scoped_ptr<base::Value> root_value(value);
  if (!callback.is_null())
    relay_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, error, root_value.release()));
}

void DocumentsService::DownloadDocument(
    const GURL& document_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  DownloadFile(
      chrome_browser_net::AppendQueryParameter(document_url,
                                               "exportFormat",
                                               GetExportFormatParam(format)),
      callback);
}

void DocumentsService::DownloadFile(const GURL& document_url,
                                    const DownloadActionCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::DownloadFileOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          document_url,
          callback,
          base::MessageLoopProxy::current()));
}

void DocumentsService::DownloadFileOnUIThread(
    const GURL& document_url,
    const DownloadActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::DownloadDocumentOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   document_url,
                   relay_proxy));
    return;
  }
  (new DownloadFileOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token(),
      document_url))->Start(
          base::Bind(&DocumentsService::OnDownloadDocumentCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     relay_proxy));
}

void DocumentsService::DownloadDocumentOnAuthRefresh(
    const DownloadActionCallback& callback,
    const GURL& document_url,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      relay_proxy->PostTask(
          FROM_HERE,
          base::Bind(callback, error, document_url, FilePath()));
    return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  DownloadFileOnUIThread(document_url, callback, relay_proxy);
}

void DocumentsService::OnDownloadDocumentCompleted(
    const DownloadActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const GURL& document_url,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
    case HTTP_UNAUTHORIZED:
      gdata_auth_service_->ClearOAuth2Token();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      DownloadFileOnUIThread(document_url, callback, relay_proxy);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    relay_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, error, document_url, file_path));
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::DeleteDocumentOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          document_url,
          callback,
          base::MessageLoopProxy::current()));
}

void DocumentsService::DeleteDocumentOnUIThread(
    const GURL& document_url,
    const EntryActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::DeleteDocumentOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   document_url,
                   relay_proxy));
    return;
  }
  (new DeleteDocumentOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token(),
      document_url))->Start(
          base::Bind(&DocumentsService::OnDeleteDocumentCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     relay_proxy));
}

void DocumentsService::DeleteDocumentOnAuthRefresh(
    const EntryActionCallback& callback,
    const GURL& document_url,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      relay_proxy->PostTask(
          FROM_HERE,
          base::Bind(callback, error, document_url));
    return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  DeleteDocumentOnUIThread(document_url, callback, relay_proxy);
}

void DocumentsService::OnDeleteDocumentCompleted(
    const EntryActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
    case HTTP_UNAUTHORIZED:
      gdata_auth_service_->ClearOAuth2Token();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      DeleteDocumentOnUIThread(document_url, callback, relay_proxy);
      return;
    default:
      break;
  }
  if (!callback.is_null())
    relay_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, error, document_url));
}

void DocumentsService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const CreateEntryCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::CreateDirectoryOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          parent_content_url,
          directory_name,
          callback,
          base::MessageLoopProxy::current()));
}

void DocumentsService::CreateDirectoryOnUIThread(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const CreateEntryCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::CreateDirectoryOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   parent_content_url,
                   directory_name,
                   callback,
                   relay_proxy));
    return;
  }
  (new CreateDirectoryOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token(),
      parent_content_url,
      directory_name))->Start(
          base::Bind(&DocumentsService::OnCreateDirectoryCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     parent_content_url,
                     directory_name,
                     callback,
                     relay_proxy));
}

void DocumentsService::CreateDirectoryOnAuthRefresh(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const CreateEntryCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null())
      relay_proxy->PostTask(
          FROM_HERE,
          base::Bind(callback, error, static_cast<base::Value*>(NULL)));
    return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  CreateDirectoryOnUIThread(parent_content_url, directory_name, callback,
                            relay_proxy);
}

void DocumentsService::OnCreateDirectoryCompleted(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const CreateEntryCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    base::Value* value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
    case HTTP_UNAUTHORIZED:
      gdata_auth_service_->ClearOAuth2Token();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      CreateDirectoryOnUIThread(parent_content_url, directory_name, callback,
                                relay_proxy);
      return;
    default:
      break;
  }

  DictionaryValue* dict_value = NULL;
  Value* entry_value = NULL;
  if (value && value->GetAsDictionary(&dict_value))
    dict_value->Get("entry", &entry_value);

  if (!callback.is_null())
    relay_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, error, entry_value));
}

void DocumentsService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::InitiateUploadOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          params,
          callback,
          base::MessageLoopProxy::current()));
}

void DocumentsService::InitiateUploadOnUIThread(
    const InitiateUploadParams& params,
    const InitiateUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params.resumable_create_media_link.is_empty()) {
    if (!callback.is_null()) {
      relay_proxy->PostTask(FROM_HERE,
                            base::Bind(callback, HTTP_BAD_REQUEST, GURL()));
    }
    return;
  }

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::InitiateUploadOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   params,
                   callback,
                   relay_proxy));
    return;
  }
  (new InitiateUploadOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token(),
      params))->Start(
          base::Bind(&DocumentsService::OnInitiateUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     params,
                     callback,
                     relay_proxy));
}

void DocumentsService::InitiateUploadOnAuthRefresh(
    const InitiateUploadParams& params,
    const InitiateUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null()) {
      relay_proxy->PostTask(FROM_HERE,
                            base::Bind(callback, error, GURL()));
    }
    return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  InitiateUploadOnUIThread(params, callback, relay_proxy);
}

void DocumentsService::OnInitiateUploadCompleted(
    const InitiateUploadParams& params,
    const InitiateUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const GURL& upload_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
    case HTTP_UNAUTHORIZED:
      gdata_auth_service_->ClearOAuth2Token();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      InitiateUploadOnUIThread(params, callback, relay_proxy);
      return;
    default:
      break;
  }
  if (!callback.is_null()) {
    relay_proxy->PostTask(FROM_HERE,
                          base::Bind(callback, error, upload_location));
  }
}

void DocumentsService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DocumentsService::ResumeUploadOnUIThread,
          weak_ptr_bound_to_ui_thread_,
          params,
          callback,
          base::MessageLoopProxy::current()));
}

void DocumentsService::ResumeUploadOnUIThread(
    const ResumeUploadParams& params,
    const ResumeUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::ResumeUploadOnAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   params,
                   callback,
                   relay_proxy));
    return;
  }

  (new ResumeUploadOperation(
      operation_registry_.get(),
      profile_,
      gdata_auth_service_->oauth2_auth_token(),
      params))->Start(
          base::Bind(&DocumentsService::OnResumeUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     params,
                     callback,
                     relay_proxy));
}

void DocumentsService::ResumeUploadOnAuthRefresh(
    const ResumeUploadParams& params,
    const ResumeUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != HTTP_SUCCESS) {
    if (!callback.is_null()) {
      relay_proxy->PostTask(FROM_HERE,
                            base::Bind(callback, error, 0, 0));
    }
    return;
  }
  DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
  ResumeUploadOnUIThread(params, callback, relay_proxy);
}

void DocumentsService::OnResumeUploadCompleted(
    const ResumeUploadParams& params,
    const ResumeUploadCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    GDataErrorCode error,
    int64 start_range_received,
    int64 end_range_received) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (error) {
    case HTTP_UNAUTHORIZED:
      gdata_auth_service_->ClearOAuth2Token();
      // User authentication might have expired - rerun the request to force
      // auth token refresh.
      ResumeUploadOnUIThread(params, callback, relay_proxy);
      return;
    default:
      break;
  }
  if (!callback.is_null()) {
    relay_proxy->PostTask(FROM_HERE,
                          base::Bind(callback,
                                     error,
                                     start_range_received,
                                     end_range_received));
  }
}

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

}  // namespace gdata
