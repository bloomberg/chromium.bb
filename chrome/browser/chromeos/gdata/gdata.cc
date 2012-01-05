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
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/common/chrome_notification_types.h"
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
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

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

const int kMaxDocumentsPerFeed = 1000;

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

}  // namespace

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
    url_fetcher_->SetExtraRequestHeaders(kGDataVersionHeader);
    url_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(kAuthorizationHeaderFormat, auth_token_.data()));
    std::vector<std::string> headers = GetExtraRequestHeaders();
    for (std::vector<std::string>::iterator iter = headers.begin();
         iter != headers.end(); ++iter) {
      url_fetcher_->SetExtraRequestHeaders(*iter);
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

  T callback_;
  Profile* profile_;
  std::string auth_token_;
  bool save_temp_file_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
};

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

  GURL GetURL() const OVERRIDE { return document_url_; }

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n"
             << source->GetResponseHeaders()->raw_headers();

    if (!callback_.is_null())
      callback_.Run(code, document_url_);

    delete this;
  }

  GURL document_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

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
    DVLOG(1) << "Response headers:\n"
             << source->GetResponseHeaders()->raw_headers();

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

  GURL GetURL() const OVERRIDE { return document_url_; }

  // content::URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n"
             << source->GetResponseHeaders()->raw_headers();

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

// static.
DocumentsService* DocumentsService::GetInstance() {
  return Singleton<DocumentsService>::get();
}

DocumentsService::DocumentsService() {
}

DocumentsService::~DocumentsService() {
}

void DocumentsService::GetDocuments(GetDataCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));
  if (!HasOAuth2AuthToken()) {
    // Fetch OAuth2 authetication token from the refresh token first.
    StartAuthentication(base::Bind(&DocumentsService::GetDocumentsOnAuthRefresh,
                                   base::Unretained(this),
                                   callback));
    return;
  }
  (new GetDocumentsOperation(profile_, GetOAuth2AuthToken()))->Start(
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
  DCHECK(HasOAuth2RefreshToken());
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
  if (!HasOAuth2AuthToken()) {
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
      GetOAuth2AuthToken(),
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
  DCHECK(HasOAuth2RefreshToken());
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
  if (!HasOAuth2AuthToken()) {
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
      GetOAuth2AuthToken(),
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
  DCHECK(HasOAuth2RefreshToken());
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

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  if (!HasOAuth2RefreshToken())
    return;

  // TODO(zelidrag): Remove this becasue we probably don't want to fetch this
  // before it is really needed.
  GetDocuments(base::Bind(&DocumentsService::UpdateFilelist,
                          base::Unretained(this)));
}

void DocumentsService::UpdateFilelist(GDataErrorCode status,
                                      base::Value* data) {
  if (status != HTTP_SUCCESS)
    return;

  DocumentFeed* feed = DocumentFeed::CreateFrom(data);
  feed_value_.reset(data);
  // TODO(zelidrag): This part should be removed once we start handling the
  // results properly.
  std::string json;
  base::JSONWriter::Write(feed_value_.get(), true, &json);
  DVLOG(1) << "Received document feed:\n" << json;
  DVLOG(1) << "Parsed feed info:"
           << "\n  title = " << feed->title()
           << "\n  start_index = " << feed->start_index()
           << "\n  items_per_page = " << feed->items_per_page();
}


}  // namespace gdata
