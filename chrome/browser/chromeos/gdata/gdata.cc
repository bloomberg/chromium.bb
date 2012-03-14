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
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "net/base/escape.h"
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

// Maximum number of attempts for re-authentication per operation.
const int kMaxReAuthenticateAttemptsPerOperation = 1;

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

//================================ AuthOperation ===============================

// OAuth2 authorization token retrieval operation.
class AuthOperation : public GDataOperationRegistry::Operation,
                      public OAuth2AccessTokenConsumer {
 public:
  AuthOperation(GDataOperationRegistry* registry,
                Profile* profile,
                const AuthStatusCallback& callback,
                const std::string& refresh_token);
  virtual ~AuthOperation() {}
  void Start();

  // Overridden from OAuth2AccessTokenConsumer:
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden from GDataOpertionRegistry::Operation
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
                             const AuthStatusCallback& callback,
                             const std::string& refresh_token)
    : GDataOperationRegistry::Operation(registry),
      profile_(profile), token_(refresh_token), callback_(callback) {
}

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
  NotifyFinish(OPERATION_SUCCESS);
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "AuthOperation: token request using refresh token failed";
  callback_.Run(HTTP_UNAUTHORIZED, std::string());
  NotifyFinish(OPERATION_FAILURE);
}

//=========================== GDataOperationInterface ==========================

// An interface for implementing an operation used by DocumentsService.
class GDataOperationInterface {
 public:
  // Callback to DocumentsService upon for re-authentication.
  typedef base::Callback<void(GDataOperationInterface* operation)>
      ReAuthenticateCallback;

  // Starts the actual operation after obtaining an authentication token
  // |auth_token|.
  virtual void Start(const std::string& auth_token) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(GDataErrorCode code) = 0;

  // Sets the callback to DocumentsService when the operation restarts due to
  // an authentication failure.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) = 0;

 protected:
  virtual ~GDataOperationInterface() {}
};

//============================== UrlFetchOperation =============================

// Base class for operations that are fetching URLs.
template <typename T>
class UrlFetchOperation : public GDataOperationInterface,
                          public GDataOperationRegistry::Operation,
                          public URLFetcherDelegate {
 public:
  typedef T CompletionCallback;

  UrlFetchOperation(GDataOperationRegistry* registry,
                    Profile* profile,
                    const CompletionCallback& callback)
      : GDataOperationRegistry::Operation(registry),
        profile_(profile),
        // Completion callback runs on the origin thread that creates
        // this operation object.
        callback_(callback),
        // MessageLoopProxy is used to run |callback| on the origin thread.
        relay_proxy_(base::MessageLoopProxy::current()),
        re_authenticate_count_(0),
        save_temp_file_(false) {
  }

  // Overridden from GDataOperationInterface.
  virtual void Start(const std::string& auth_token) OVERRIDE {
    DCHECK(!auth_token.empty());

    GURL url = GetURL();
    DCHECK(!url.is_empty());
    DVLOG(1) << "URL: " << url.spec();

    url_fetcher_.reset(URLFetcher::Create(url, GetRequestType(), this));
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
    NotifyStart();

    url_fetcher_->Start();
  }

  // Overridden from GDataOperationInterface.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) OVERRIDE {
    DCHECK(re_authenticate_callback_.is_null());

    re_authenticate_callback_ = callback;
  }

 protected:
  virtual ~UrlFetchOperation() {}
  // Gets URL for the request.
  virtual GURL GetURL() const = 0;
  // Returns the request type. A derived class should override this method
  // for a request type other than HTTP GET.
  virtual URLFetcher::RequestType GetRequestType() const {
    return URLFetcher::GET;
  }
  // Returns the extra HTTP headers for the request. A derived class should
  // override this method to specify any extra headers needed for the request.
  virtual std::vector<std::string> GetExtraRequestHeaders() const {
    return std::vector<std::string>();
  }
  // Used by a derived class to add any content data to the request.
  // Returns true if |upload_content_type| and |upload_content| are updated
  // with the content type and data for the request.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) {
    return false;
  }

  // Invoked by OnURLFetchComplete when the operation completes without an
  // authentication error. Must be implemented by a derived class.
  virtual void ProcessURLFetchResults(const URLFetcher* source) = 0;

  // Invoked by this base class upon an authentication error.
  // Must be implemented by a derived class.
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) = 0;

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

  // Overridden from URLFetcherDelegate.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
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
    ProcessURLFetchResults(source);
    NotifyFinish(OPERATION_SUCCESS);
  }

  // Overridden from GDataOperationInterface.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE {
    RunCallbackOnAuthFailed(code);
    NotifyFinish(OPERATION_FAILURE);
  }

  Profile* profile_;
  CompletionCallback callback_;
  ReAuthenticateCallback re_authenticate_callback_;
  scoped_refptr<base::MessageLoopProxy> relay_proxy_;
  int re_authenticate_count_;
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
                       const EntryActionCallback& callback,
                       const GURL& document_url)
      : UrlFetchOperation<EntryActionCallback>(registry, profile, callback),
        document_url_(document_url) {
  }

 protected:
  virtual ~EntryActionOperation() {}

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE {
    return AddStandardUrlParams(document_url_);
  }

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
    if (!callback_.is_null()) {
      GDataErrorCode code =
          static_cast<GDataErrorCode>(source->GetResponseCode());
      relay_proxy_->PostTask(FROM_HERE,
                             base::Bind(callback_, code, document_url_));
    }
  }

  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE {
    if (!callback_.is_null()) {
      relay_proxy_->PostTask(FROM_HERE,
                             base::Bind(callback_, code, document_url_));
    }
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
                   const GetDataCallback& callback)
      : UrlFetchOperation<GetDataCallback>(registry, profile, callback) {
  }

 protected:
  virtual ~GetDataOperation() {}

  // Overridden from UrlFetchOperation.
  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
    std::string data;
    source->GetResponseAsString(&data);
    scoped_ptr<base::Value> root_value;
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());

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

  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE {
    if (!callback_.is_null()) {
      scoped_ptr<base::Value> root_value;
      relay_proxy_->PostTask(
          FROM_HERE,
          base::Bind(callback_, code, base::Passed(&root_value)));
    }
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
class GetDocumentsOperation : public GetDataOperation {
 public:
  GetDocumentsOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const GetDataCallback& callback);
  virtual ~GetDocumentsOperation() {}
  // Sets |url| for document fetching operation. This URL should be set in use
  // case when additional 'pages' of document lists are being fetched.
  void SetUrl(const GURL& url);

 private:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

  GURL override_url_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
};

GetDocumentsOperation::GetDocumentsOperation(GDataOperationRegistry* registry,
                                             Profile* profile,
                                             const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback) {
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
                        const DownloadActionCallback& callback,
                        const GURL& document_url)
      : UrlFetchOperation<DownloadActionCallback>(registry,
                                                  profile,
                                                  callback),
        document_url_(document_url) {
    // Make sure we download the content into a temp file.
    save_temp_file_ = true;
  }

 protected:
  virtual ~DownloadFileOperation() {}

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE { return document_url_; }

  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());

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

  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE {
    if (!callback_.is_null()) {
      relay_proxy_->PostTask(
          FROM_HERE,
          base::Bind(callback_, code, document_url_, FilePath()));
    }
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// Document list fetching operation.
class DeleteDocumentOperation : public EntryActionOperation {
 public:
  DeleteDocumentOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const EntryActionCallback& callback,
                          const GURL& document_url);
  virtual ~DeleteDocumentOperation() {}

 private:
  // Overridden from EntryActionOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
};

DeleteDocumentOperation::DeleteDocumentOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& document_url)
    : EntryActionOperation(registry, profile, callback, document_url) {
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

class CreateDirectoryOperation : public GetDataOperation {
 public:
  // Empty |parent_content_url| will create the directory in the root folder.
  CreateDirectoryOperation(GDataOperationRegistry* registry,
                           Profile* profile,
                           const GetDataCallback& callback,
                           const GURL& parent_content_url,
                           const FilePath::StringType& directory_name);

 protected:
  virtual ~CreateDirectoryOperation() {}

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from URLFetcherDelegate.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;

 private:
  // Overridden from UrlFetchOperation.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  const GURL parent_content_url_;
  const FilePath::StringType directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

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

//=========================== CopyDocumentOperation ============================

// Operation for making a copy of a document.
class CopyDocumentOperation : public GetDataOperation {
 public:
  CopyDocumentOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const GetDataCallback& callback,
                        const GURL& document_url,
                        const FilePath::StringType& new_name);
  virtual ~CopyDocumentOperation() {}

 private:
  // Overridden from GetDataOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  GURL document_url_;
  FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(CopyDocumentOperation);
};

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

class RenameResourceOperation : public EntryActionOperation {
 public:
  RenameResourceOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const EntryActionCallback& callback,
                          const GURL& document_url,
                          const FilePath::StringType& new_name);
  virtual ~RenameResourceOperation() {}

 private:
  // Overridden from EntryActionOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
};

RenameResourceOperation::RenameResourceOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& document_url,
    const FilePath::StringType& new_name)
    : EntryActionOperation(registry, profile, callback, document_url),
      new_name_(new_name) {
}

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

//=========================== AddResourceToDirectoryOperation ==================

class AddResourceToDirectoryOperation : public EntryActionOperation {
 public:
  AddResourceToDirectoryOperation(GDataOperationRegistry* registry,
                                  Profile* profile,
                                  const EntryActionCallback& callback,
                                  const GURL& parent_content_url,
                                  const GURL& document_url);
  virtual ~AddResourceToDirectoryOperation() {}

 private:
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  const GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(AddResourceToDirectoryOperation);
};

AddResourceToDirectoryOperation::AddResourceToDirectoryOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const EntryActionCallback& callback,
    const GURL& parent_content_url,
    const GURL& document_url)
    : EntryActionOperation(registry, profile, callback, document_url),
      parent_content_url_(parent_content_url) {
}

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

  xml_writer.WriteElement("id", document_url_.spec());

  xml_writer.EndElement();  // Ends "entry" element.
  xml_writer.StopWriting();
  upload_content->assign(xml_writer.GetWrittenString());
  DVLOG(1) << "AddResourceToDirectoryOperation data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//=========================== RemoveResourceFromDirectoryOperation =============

class RemoveResourceFromDirectoryOperation : public EntryActionOperation {
 public:
  RemoveResourceFromDirectoryOperation(GDataOperationRegistry* registry,
                                       Profile* profile,
                                       const EntryActionCallback& callback,
                                       const GURL& parent_content_url,
                                       const GURL& document_url,
                                       const std::string& resource_id);
  virtual ~RemoveResourceFromDirectoryOperation() {}

 private:
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  const std::string resource_id_;
  const GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryOperation);
};

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

class InitiateUploadOperation
    : public UrlFetchOperation<InitiateUploadCallback> {
 public:
  InitiateUploadOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const InitiateUploadCallback& callback,
                          const InitiateUploadParams& params);

 protected:
  virtual ~InitiateUploadOperation() {}

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

 private:
  // Overridden from UrlFetchOperation.
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
    const InitiateUploadCallback& callback,
    const InitiateUploadParams& params)
    : UrlFetchOperation<InitiateUploadCallback>(registry, profile, callback),
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

class ResumeUploadOperation : public UrlFetchOperation<ResumeUploadCallback> {
 public:
  ResumeUploadOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const ResumeUploadCallback& callback,
                        const ResumeUploadParams& params);

 protected:
  virtual ~ResumeUploadOperation() {}

  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual void ProcessURLFetchResults(const URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

 private:
  // Overridden from UrlFetchOperation.
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
    const ResumeUploadCallback& callback,
    const ResumeUploadParams& params)
    : UrlFetchOperation<ResumeUploadCallback>(registry, profile, callback),
      params_(params) {
}

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
  *upload_content =
      std::string(params_.buf->data(),
                  params_.end_range - params_.start_range + 1);
  return true;
}

//================================ GDataAuthService ============================

void GDataAuthService::Initialize(Profile* profile) {
  profile_ = profile;
  // Get OAuth2 refresh token (if we have any) and register for its updates.
  TokenService* service = TokenServiceFactory::GetForProfile(profile_);
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
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataAuthService::~GDataAuthService() {
}

void GDataAuthService::StartAuthentication(
    GDataOperationRegistry* registry,
    const AuthStatusCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> relay_proxy(
      base::MessageLoopProxy::current());

  if (IsFullyAuthenticated()) {
    relay_proxy->PostTask(FROM_HERE,
         base::Bind(callback, gdata::HTTP_SUCCESS, oauth2_auth_token()));
  } else if (IsPartiallyAuthenticated()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataAuthService::StartAuthenticationOnUIThread,
                   weak_ptr_bound_to_ui_thread_,
                   registry,
                   relay_proxy,
                   base::Bind(&GDataAuthService::OnAuthCompleted,
                              weak_ptr_bound_to_ui_thread_,
                              relay_proxy,
                              callback)));
  } else {
    relay_proxy->PostTask(FROM_HERE,
        base::Bind(callback, gdata::HTTP_UNAUTHORIZED, std::string()));
  }
}

void GDataAuthService::StartAuthenticationOnUIThread(
    GDataOperationRegistry* registry,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We have refresh token, let's gets authenticated.
  (new AuthOperation(registry, profile_,
                     callback, GetOAuth2RefreshToken()))->Start();
}

void GDataAuthService::OnAuthCompleted(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback,
    GDataErrorCode error,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == HTTP_SUCCESS)
    auth_token_ = auth_token;

  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  if (!callback.is_null())
    relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error, auth_token));
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
    TokenService* service = TokenServiceFactory::GetForProfile(profile_);
    refresh_token_ = service->GetOAuth2LoginRefreshToken();
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
      weak_ptr_factory_(this),
      // The weak pointers is used to post tasks to UI thread.
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  gdata_auth_service_->StartAuthentication(operation_registry_.get(),
                                           callback);
}

void DocumentsService::GetDocuments(const GURL& url,
                                    const GetDataCallback& callback) {
  GetDocumentsOperation* operation =
      new GetDocumentsOperation(operation_registry_.get(), profile_, callback);
  if (!url.is_empty())
    operation->SetUrl(url);
  StartOperationOnUIThread(operation);
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
  StartOperationOnUIThread(
      new DownloadFileOperation(operation_registry_.get(), profile_, callback,
                                document_url));
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new DeleteDocumentOperation(operation_registry_.get(), profile_, callback,
                                  document_url));
}

void DocumentsService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  StartOperationOnUIThread(
      new CreateDirectoryOperation(operation_registry_.get(), profile_,
                                   callback, parent_content_url,
                                   directory_name));
}

void DocumentsService::CopyDocument(const GURL& document_url,
                                    const FilePath::StringType& new_name,
                                    const GetDataCallback& callback) {
  StartOperationOnUIThread(
      new CopyDocumentOperation(operation_registry_.get(), profile_, callback,
                                document_url, new_name));
}

void DocumentsService::RenameResource(const GURL& resource_url,
                                      const FilePath::StringType& new_name,
                                      const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new RenameResourceOperation(operation_registry_.get(), profile_, callback,
                                  resource_url, new_name));
}

void DocumentsService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new AddResourceToDirectoryOperation(operation_registry_.get(),
                                          profile_,
                                          callback,
                                          parent_content_url,
                                          resource_url));
}

void DocumentsService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new RemoveResourceFromDirectoryOperation(operation_registry_.get(),
                                               profile_,
                                               callback,
                                               parent_content_url,
                                               resource_url,
                                               resource_id));
}

void DocumentsService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  if (params.resumable_create_media_link.is_empty()) {
    if (!callback.is_null()) {
      callback.Run(HTTP_BAD_REQUEST, GURL());
    }
    return;
  }

  StartOperationOnUIThread(
      new InitiateUploadOperation(operation_registry_.get(), profile_, callback,
                                  params));
}

void DocumentsService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  StartOperationOnUIThread(
      new ResumeUploadOperation(operation_registry_.get(), profile_, callback,
                                params));
}

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DocumentsService::StartOperationOnUIThread(
    GDataOperationInterface* operation) {
  operation->SetReAuthenticateCallback(
      base::Bind(&DocumentsService::RetryOperation,
                 weak_ptr_factory_.GetWeakPtr()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DocumentsService::StartOperation,
                 weak_ptr_bound_to_ui_thread_,
                 operation));  // |operation| is self-contained
}

void DocumentsService::StartOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authentication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::OnOperationAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   operation));
    return;
  }

  operation->Start(gdata_auth_service_->oauth2_auth_token());
}

void DocumentsService::OnOperationAuthRefresh(
    GDataOperationInterface* operation,
    GDataErrorCode code,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code == HTTP_SUCCESS) {
    DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
    StartOperation(operation);
  } else {
    operation->OnAuthFailed(code);
  }
}

void DocumentsService::RetryOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata_auth_service_->ClearOAuth2Token();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartOperation(operation);
}

}  // namespace gdata
