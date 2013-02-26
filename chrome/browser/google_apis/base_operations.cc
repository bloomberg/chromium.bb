// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_operations.h"

#include "base/json/json_reader.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
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

// Template for initiate upload of both GData WAPI and Drive API v2.
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";
const char kUploadResponseLocation[] = "location";

// Template for upload data range of both GData WAPI and Drive API v2.
const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadResponseRange[] = "range";

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
    const base::FilePath& path)
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
  if (url.is_empty()) {
    // Error is found on generating the url. Send the error message to the
    // callback, and then return immediately without trying to connect
    // to the server.
    RunCallbackOnPrematureFailure(GDATA_OTHER_ERROR);
    return;
  }
  DVLOG(1) << "URL: " << url.spec();

  URLFetcher::RequestType request_type = GetRequestType();
  url_fetcher_.reset(
      URLFetcher::Create(url, request_type, this));
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
  } else {
    // Even if there is no content data, UrlFetcher requires to set empty
    // upload data string for POST, PUT and PATCH methods, explicitly.
    // It is because that most requests of those methods have non-empty
    // body, and UrlFetcher checks whether it is actually not forgotten.
    if (request_type == URLFetcher::POST ||
        request_type == URLFetcher::PUT ||
        request_type == URLFetcher::PATCH) {
      // Set empty upload content-type and upload content, so that
      // the request will have no "Content-type: " header and no content.
      url_fetcher_->SetUploadData("", "");
    }
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

//========================= InitiateUploadOperationBase ========================

InitiateUploadOperationBase::InitiateUploadOperationBase(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const InitiateUploadCallback& callback,
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length)
    : UrlFetchOperationBase(registry,
                            url_request_context_getter,
                            OPERATION_UPLOAD,
                            drive_file_path),
      callback_(callback),
      drive_file_path_(drive_file_path),
      content_type_(content_type),
      content_length_(content_length) {
  DCHECK(!callback_.is_null());
  DCHECK(!content_type_.empty());
  DCHECK_GE(content_length_, 0);
}

InitiateUploadOperationBase::~InitiateUploadOperationBase() {}

void InitiateUploadOperationBase::ProcessURLFetchResults(
    const URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);

  std::string upload_location;
  if (code == HTTP_SUCCESS) {
    // Retrieve value of the first "Location" header.
    source->GetResponseHeaders()->EnumerateHeader(NULL,
                                                  kUploadResponseLocation,
                                                  &upload_location);
  }
  VLOG(1) << "Got response for [" << drive_file_path_.value()
          << "]: code=" << code
          << ", location=[" << upload_location << "]";

  callback_.Run(code, GURL(upload_location));
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void InitiateUploadOperationBase::NotifySuccessToOperationRegistry() {
  NotifySuspend();
}

void InitiateUploadOperationBase::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  callback_.Run(code, GURL());
}

std::vector<std::string>
InitiateUploadOperationBase::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kUploadContentType + content_type_);
  headers.push_back(
      kUploadContentLength + base::Int64ToString(content_length_));
  return headers;
}

//========================== UploadRangeOperationBase ==========================

UploadRangeOperationBase::UploadRangeOperationBase(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url)
    : UrlFetchOperationBase(registry,
                            url_request_context_getter,
                            OPERATION_UPLOAD,
                            drive_file_path),
      upload_mode_(upload_mode),
      drive_file_path_(drive_file_path),
      upload_url_(upload_url),
      last_chunk_completed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

UploadRangeOperationBase::~UploadRangeOperationBase() {}

GURL UploadRangeOperationBase::GetURL() const {
  // This is very tricky to get json from this operation. To do that, &alt=json
  // has to be appended not here but in InitiateUploadOperation::GetURL().
  return upload_url_;
}

URLFetcher::RequestType UploadRangeOperationBase::GetRequestType() const {
  return URLFetcher::PUT;
}

void UploadRangeOperationBase::ProcessURLFetchResults(
    const URLFetcher* source) {
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
    DVLOG(1) << "Got response for [" << drive_file_path_.value()
             << "]: code=" << code
             << ", range_hdr=[" << range_received
             << "], range_parsed=" << start_position_received
             << "," << end_position_received;

    OnRangeOperationComplete(UploadRangeResponse(code,
                                                 start_position_received,
                                                 end_position_received),
                             scoped_ptr<base::Value>());

    OnProcessURLFetchResultsComplete(true);
  } else {
    // There might be explanation of unexpected error code in response.
    std::string response_content;
    source->GetResponseAsString(&response_content);
    DVLOG(1) << "Got response for [" << drive_file_path_.value()
             << "]: code=" << code
             << ", content=[\n" << response_content << "\n]";

    ParseJson(response_content,
              base::Bind(&UploadRangeOperationBase::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr(),
                         code));
  }
}

void UploadRangeOperationBase::OnDataParsed(GDataErrorCode code,
                                            scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // For a new file, HTTP_CREATED is returned.
  // For an existing file, HTTP_SUCCESS is returned.
  if ((upload_mode_ == UPLOAD_NEW_FILE && code == HTTP_CREATED) ||
      (upload_mode_ == UPLOAD_EXISTING_FILE && code == HTTP_SUCCESS)) {
    last_chunk_completed_ = true;
  }

  OnRangeOperationComplete(UploadRangeResponse(code, -1, -1), value.Pass());
  OnProcessURLFetchResultsComplete(last_chunk_completed_);
}

void UploadRangeOperationBase::NotifyStartToOperationRegistry() {
  NotifyResume();
}

void UploadRangeOperationBase::NotifySuccessToOperationRegistry() {
  if (last_chunk_completed_)
    NotifyFinish(OPERATION_COMPLETED);
  else
    NotifySuspend();
}

void UploadRangeOperationBase::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  OnRangeOperationComplete(
      UploadRangeResponse(code, 0, 0), scoped_ptr<base::Value>());
}

//============================ DownloadFileOperation ===========================

DownloadFileOperation::DownloadFileOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const GURL& download_url,
    const base::FilePath& drive_file_path,
    const base::FilePath& output_file_path)
    : UrlFetchOperationBase(registry,
                            url_request_context_getter,
                            OPERATION_DOWNLOAD,
                            drive_file_path),
      download_action_callback_(download_action_callback),
      get_content_callback_(get_content_callback),
      download_url_(download_url) {
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
  return download_url_;
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
  base::FilePath temp_file;
  if (code == HTTP_SUCCESS &&
      !source->GetResponseAsFilePath(true,  // take_ownership
                                     &temp_file)) {
    code = GDATA_FILE_ERROR;
  }

  download_action_callback_.Run(code, temp_file);
  OnProcessURLFetchResultsComplete(code == HTTP_SUCCESS);
}

void DownloadFileOperation::RunCallbackOnPrematureFailure(GDataErrorCode code) {
  download_action_callback_.Run(code, base::FilePath());
}

}  // namespace google_apis
