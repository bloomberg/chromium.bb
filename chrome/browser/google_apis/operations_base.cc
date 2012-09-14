// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/operations_base.h"

#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;
using net::URLFetcher;

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

// Maximum number of attempts for re-authentication per operation.
const int kMaxReAuthenticateAttemptsPerOperation = 1;

// Parse JSON string to base::Value object.
void ParseJsonOnBlockingPool(const std::string& data,
                             scoped_ptr<base::Value>* value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  int error_code = -1;
  std::string error_message;
  value->reset(base::JSONReader::ReadAndReturnError(data,
                                                    base::JSON_PARSE_RFC,
                                                    &error_code,
                                                    &error_message));

  if (!value->get()) {
    LOG(ERROR) << "Error while parsing entry response: "
               << error_message
               << ", code: "
               << error_code
               << ", data:\n"
               << data;
  }
}

}  // namespace

namespace gdata {

//================================ AuthOperation ===============================

AuthOperation::AuthOperation(OperationRegistry* registry,
                             const AuthStatusCallback& callback,
                             const std::vector<std::string>& scopes,
                             const std::string& refresh_token)
    : OperationRegistry::Operation(registry),
      refresh_token_(refresh_token),
      callback_(callback),
      scopes_(scopes) {
}

AuthOperation::~AuthOperation() {}

void AuthOperation::Start() {
  DCHECK(!refresh_token_.empty());
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, g_browser_process->system_request_context()));
  NotifyStart();
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      refresh_token_,
      scopes_);
}

void AuthOperation::DoCancel() {
  oauth2_access_token_fetcher_->CancelRequest();
  if (!callback_.is_null())
    callback_.Run(GDATA_CANCELLED, std::string());
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void AuthOperation::OnGetTokenSuccess(const std::string& access_token,
                                      const base::Time& expiration_time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                            kSuccessRatioHistogramSuccess,
                            kSuccessRatioHistogramMaxValue);

  callback_.Run(HTTP_SUCCESS, access_token);
  NotifyFinish(OperationRegistry::OPERATION_COMPLETED);
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
  NotifyFinish(OperationRegistry::OPERATION_FAILED);
}

//============================ UrlFetchOperationBase ===========================

UrlFetchOperationBase::UrlFetchOperationBase(OperationRegistry* registry)
    : OperationRegistry::Operation(registry),
      re_authenticate_count_(0),
      save_temp_file_(false),
      started_(false) {
}

UrlFetchOperationBase::UrlFetchOperationBase(
    OperationRegistry* registry,
    OperationRegistry::OperationType type,
    const FilePath& path)
    : OperationRegistry::Operation(registry, type, path),
      re_authenticate_count_(0),
      save_temp_file_(false) {
}

UrlFetchOperationBase::~UrlFetchOperationBase() {}

void UrlFetchOperationBase::Start(const std::string& auth_token) {
  DCHECK(!auth_token.empty());

  GURL url = GetURL();
  DCHECK(!url.is_empty());
  DVLOG(1) << "URL: " << url.spec();

  url_fetcher_.reset(
      URLFetcher::Create(url, GetRequestType(), this));
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

GDataErrorCode UrlFetchOperationBase::GetErrorCode(
    const URLFetcher* source) const {
  GDataErrorCode code = static_cast<GDataErrorCode>(source->GetResponseCode());
  if (code == HTTP_SUCCESS && !source->GetStatus().is_success()) {
    // If the HTTP response code is SUCCESS yet the URL request failed, it is
    // likely that the failure is due to loss of connection.
    code = GDATA_NO_CONNECTION;
  }
  return code;
}

void UrlFetchOperationBase::OnProcessURLFetchResultsComplete(bool result) {
  if (result)
    NotifySuccessToOperationRegistry();
  else
    NotifyFinish(OperationRegistry::OPERATION_FAILED);
}

void UrlFetchOperationBase::OnURLFetchComplete(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
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
}

void UrlFetchOperationBase::NotifySuccessToOperationRegistry() {
  NotifyFinish(OperationRegistry::OPERATION_COMPLETED);
}

void UrlFetchOperationBase::NotifyStartToOperationRegistry() {
  NotifyStart();
}

void UrlFetchOperationBase::OnAuthFailed(GDataErrorCode code) {
  RunCallbackOnPrematureFailure(code);

  // Notify authentication failed.
  NotifyAuthFailed();

  // Check if this failed before we even started fetching. If so, register
  // for start so we can properly unregister with finish.
  if (!started_)
    NotifyStart();

  // Note: NotifyFinish() must be invoked at the end, after all other callbacks
  // and notifications. Once NotifyFinish() is called, the current instance of
  // gdata operation will be deleted from the OperationRegistry and become
  // invalid.
  NotifyFinish(OperationRegistry::OPERATION_FAILED);
}

std::string UrlFetchOperationBase::GetResponseHeadersAsString(
    const URLFetcher* url_fetcher) {
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

EntryActionOperation::EntryActionOperation(OperationRegistry* registry,
                                           const EntryActionCallback& callback,
                                           const GURL& document_url)
    : UrlFetchOperationBase(registry),
      callback_(callback),
      document_url_(document_url) {
}

EntryActionOperation::~EntryActionOperation() {}

void EntryActionOperation::ProcessURLFetchResults(const URLFetcher* source) {
  if (!callback_.is_null()) {
    GDataErrorCode code = GetErrorCode(source);
    callback_.Run(code, document_url_);
  }
  const bool success = true;
  OnProcessURLFetchResultsComplete(success);
}

void EntryActionOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  if (!callback_.is_null())
    callback_.Run(code, document_url_);
}

//============================== GetDataOperation ==============================

GetDataOperation::GetDataOperation(OperationRegistry* registry,
                                   const GetDataCallback& callback)
    : UrlFetchOperationBase(registry),
      callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

GetDataOperation::~GetDataOperation() {}

void GetDataOperation::ProcessURLFetchResults(const URLFetcher* source) {
  std::string data;
  source->GetResponseAsString(&data);
  scoped_ptr<base::Value> root_value;
  GDataErrorCode fetch_error_code = GetErrorCode(source);

  switch (fetch_error_code) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      ParseResponse(fetch_error_code, data);
      break;
    default:
      RunCallback(fetch_error_code, scoped_ptr<base::Value>());
      const bool success = false;
      OnProcessURLFetchResultsComplete(success);
      break;
  }
}

void GetDataOperation::RunCallbackOnPrematureFailure(
    GDataErrorCode fetch_error_code) {
  if (!callback_.is_null()) {
    scoped_ptr<base::Value> root_value;
    callback_.Run(fetch_error_code, root_value.Pass());
  }
}

void GetDataOperation::ParseResponse(GDataErrorCode fetch_error_code,
                                     const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Uses this hack to avoid deep-copy of json object because json might be so
  // big. This pointer of scped_ptr is to ensure a deletion of the parsed json
  // value object.
  scoped_ptr<base::Value>* parsed_value = new scoped_ptr<base::Value>();

  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&ParseJsonOnBlockingPool,
                 data,
                 parsed_value),
      base::Bind(&GetDataOperation::OnDataParsed,
                 weak_ptr_factory_.GetWeakPtr(),
                 fetch_error_code,
                 base::Owned(parsed_value)));
}

void GetDataOperation::OnDataParsed(
    gdata::GDataErrorCode fetch_error_code,
    scoped_ptr<base::Value>* value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool success = true;
  if (!value->get()) {
    fetch_error_code = gdata::GDATA_PARSE_ERROR;
    success = false;
  }

  // The ownership of the parsed json object is transfered to RunCallBack(),
  // keeping the ownership of the |value| here.
  RunCallback(fetch_error_code, value->Pass());

  DCHECK(!value->get());

  OnProcessURLFetchResultsComplete(success);
  // |value| will be deleted after return because it is base::Owned()'d.
}

void GetDataOperation::RunCallback(GDataErrorCode fetch_error_code,
                                   scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback_.is_null())
    callback_.Run(fetch_error_code, value.Pass());
}

}  // namespace gdata
