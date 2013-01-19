// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_operations.h"

#include "base/json/json_reader.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;
using net::URLFetcher;

namespace {

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// Maximum number of attempts for re-authentication per operation.
const int kMaxReAuthenticateAttemptsPerOperation = 1;

// Parse JSON string to base::Value object.
scoped_ptr<base::Value> ParseJsonOnBlockingPool(const std::string& json) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &error_message));

  if (!value.get()) {
    LOG(ERROR) << "Error while parsing entry response: " << error_message
               << ", code: " << error_code << ", json:\n" << json;
  }
  return value.Pass();
}

// Returns response headers as a string. Returns a warning message if
// |url_fetcher| does not contain a valid response. Used only for debugging.
std::string GetResponseHeadersAsString(
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

}  // namespace

namespace google_apis {

void ParseJson(const std::string& json, const ParseJsonCallback& callback) {
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ParseJsonOnBlockingPool, json),
      callback);
}

//============================ UrlFetchOperationBase ===========================

UrlFetchOperationBase::UrlFetchOperationBase(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter)
    : OperationRegistry::Operation(registry),
      url_request_context_getter_(url_request_context_getter),
      re_authenticate_count_(0),
      started_(false),
      save_temp_file_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

UrlFetchOperationBase::UrlFetchOperationBase(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    OperationType type,
    const FilePath& path)
    : OperationRegistry::Operation(registry, type, path),
      url_request_context_getter_(url_request_context_getter),
      re_authenticate_count_(0),
      started_(false),
      save_temp_file_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

UrlFetchOperationBase::~UrlFetchOperationBase() {}

void UrlFetchOperationBase::Start(const std::string& access_token,
                                  const std::string& custom_user_agent,
                                  const ReAuthenticateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url_request_context_getter_);
  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(re_authenticate_callback_.is_null());

  re_authenticate_callback_ = callback;

  GURL url = GetURL();
  DCHECK(!url.is_empty());
  DVLOG(1) << "URL: " << url.spec();

  url_fetcher_.reset(
      URLFetcher::Create(url, GetRequestType(), this));
  url_fetcher_->SetRequestContext(url_request_context_getter_);
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
  if (!custom_user_agent.empty())
    url_fetcher_->AddExtraRequestHeader("User-Agent: " + custom_user_agent);
  url_fetcher_->AddExtraRequestHeader(kGDataVersionHeader);
  url_fetcher_->AddExtraRequestHeader(
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.data()));
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

// static
GDataErrorCode UrlFetchOperationBase::GetErrorCode(const URLFetcher* source) {
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
    NotifyFinish(OPERATION_FAILED);
}

void UrlFetchOperationBase::OnURLFetchComplete(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
  DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

  if (code == HTTP_UNAUTHORIZED) {
    if (++re_authenticate_count_ <= kMaxReAuthenticateAttemptsPerOperation) {
      // Reset re_authenticate_callback_ so Start() can be called again.
      ReAuthenticateCallback callback = re_authenticate_callback_;
      re_authenticate_callback_.Reset();
      callback.Run(this);
      return;
    }

    OnAuthFailed(code);
    return;
  }

  // Overridden by each specialization
  ProcessURLFetchResults(source);
}

void UrlFetchOperationBase::NotifySuccessToOperationRegistry() {
  NotifyFinish(OPERATION_COMPLETED);
}

void UrlFetchOperationBase::NotifyStartToOperationRegistry() {
  NotifyStart();
}

void UrlFetchOperationBase::OnAuthFailed(GDataErrorCode code) {
  RunCallbackOnPrematureFailure(code);

  // Notify authentication failed.
  NotifyAuthFailed(code);

  // Check if this failed before we even started fetching. If so, register
  // for start so we can properly unregister with finish.
  if (!started_)
    NotifyStart();

  // Note: NotifyFinish() must be invoked at the end, after all other callbacks
  // and notifications. Once NotifyFinish() is called, the current instance of
  // gdata operation will be deleted from the OperationRegistry and become
  // invalid.
  NotifyFinish(OPERATION_FAILED);
}

base::WeakPtr<AuthenticatedOperationInterface>
UrlFetchOperationBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}


//============================ EntryActionOperation ============================

EntryActionOperation::EntryActionOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const EntryActionCallback& callback)
    : UrlFetchOperationBase(registry, url_request_context_getter),
      callback_(callback) {
  DCHECK(!callback_.is_null());
}

EntryActionOperation::~EntryActionOperation() {}

void EntryActionOperation::ProcessURLFetchResults(const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
  callback_.Run(code);
  const bool success = true;
  OnProcessURLFetchResultsComplete(success);
}

void EntryActionOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  callback_.Run(code);
}

//============================== GetDataOperation ==============================

GetDataOperation::GetDataOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const GetDataCallback& callback)
    : UrlFetchOperationBase(registry, url_request_context_getter),
      callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(!callback_.is_null());
}

GetDataOperation::~GetDataOperation() {}

void GetDataOperation::ParseResponse(GDataErrorCode fetch_error_code,
                                     const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "JSON received from " << GetURL().spec() << ": "
          << data.size() << " bytes";
  ParseJson(data,
            base::Bind(&GetDataOperation::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr(),
                       fetch_error_code));
}

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
      RunCallbackOnPrematureFailure(fetch_error_code);
      const bool success = false;
      OnProcessURLFetchResultsComplete(success);
      break;
  }
}

void GetDataOperation::RunCallbackOnPrematureFailure(
    GDataErrorCode fetch_error_code) {
  callback_.Run(fetch_error_code, scoped_ptr<base::Value>());
}

void GetDataOperation::OnDataParsed(
    GDataErrorCode fetch_error_code,
    scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool success = true;
  if (!value.get()) {
    fetch_error_code = GDATA_PARSE_ERROR;
    success = false;
  }

  RunCallbackOnSuccess(fetch_error_code, value.Pass());

  DCHECK(!value.get());
  OnProcessURLFetchResultsComplete(success);
}

void GetDataOperation::RunCallbackOnSuccess(GDataErrorCode fetch_error_code,
                                            scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_.Run(fetch_error_code, value.Pass());
}

}  // namespace google_apis
