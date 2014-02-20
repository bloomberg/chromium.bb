// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/registration_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace gcm {

namespace {

const char kRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";
const char kRegistrationRequestContentType[] =
    "application/x-www-form-urlencoded";

// Request constants.
const char kAppIdKey[] = "app";
const char kCertKey[] = "cert";
const char kDeviceIdKey[] = "device";
const char kLoginHeader[] = "AidLogin";
const char kSenderKey[] = "sender";

// Request validation constants.
const size_t kMaxSenders = 100;

// Response constants.
const char kErrorPrefix[] = "Error=";
const char kTokenPrefix[] = "token=";
const char kDeviceRegistrationError[] = "PHONE_REGISTRATION_ERROR";
const char kAuthenticationFailed[] = "AUTHENTICATION_FAILED";
const char kInvalidSender[] = "INVALID_SENDER";
const char kInvalidParameters[] = "INVALID_PARAMETERS";

void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + net::EscapeUrlEncodedData(value, true));
}

// Gets correct status from the error message.
RegistrationRequest::Status GetStatusFromError(const std::string& error) {
  if (error == kDeviceRegistrationError)
    return RegistrationRequest::DEVICE_REGISTRATION_ERROR;
  if (error == kAuthenticationFailed)
    return RegistrationRequest::AUTHENTICATION_FAILED;
  if (error == kInvalidSender)
    return RegistrationRequest::INVALID_SENDER;
  if (error == kInvalidParameters)
    return RegistrationRequest::INVALID_PARAMETERS;
  return RegistrationRequest::UNKNOWN_ERROR;
}

// Indicates whether a retry attempt should be made based on the status of the
// last request.
bool ShouldRetryWithStatus(RegistrationRequest::Status status) {
  return status == RegistrationRequest::UNKNOWN_ERROR ||
         status == RegistrationRequest::AUTHENTICATION_FAILED ||
         status == RegistrationRequest::DEVICE_REGISTRATION_ERROR ||
         status == RegistrationRequest::HTTP_NOT_OK ||
         status == RegistrationRequest::URL_FETCHING_FAILED ||
         status == RegistrationRequest::RESPONSE_PARSING_FAILED;
}

void RecordRegistrationStatusToUMA(RegistrationRequest::Status status) {
  UMA_HISTOGRAM_ENUMERATION("GCM.RegistrationRequestStatus", status,
                            RegistrationRequest::STATUS_COUNT);
}

}  // namespace

RegistrationRequest::RequestInfo::RequestInfo(
    uint64 android_id,
    uint64 security_token,
    const std::string& app_id,
    const std::string& cert,
    const std::vector<std::string>& sender_ids)
    : android_id(android_id),
      security_token(security_token),
      app_id(app_id),
      cert(cert),
      sender_ids(sender_ids) {
}

RegistrationRequest::RequestInfo::~RequestInfo() {}

RegistrationRequest::RegistrationRequest(
    const RequestInfo& request_info,
    const net::BackoffEntry::Policy& backoff_policy,
    const RegistrationCallback& callback,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : callback_(callback),
      request_info_(request_info),
      backoff_entry_(&backoff_policy),
      request_context_getter_(request_context_getter),
      weak_ptr_factory_(this) {
}

RegistrationRequest::~RegistrationRequest() {}

void RegistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(request_info_.android_id != 0UL);
  DCHECK(request_info_.security_token != 0UL);
  DCHECK(!request_info_.cert.empty());
  DCHECK(0 < request_info_.sender_ids.size() &&
         request_info_.sender_ids.size() <= kMaxSenders);

  DCHECK(!url_fetcher_.get());
  url_fetcher_.reset(net::URLFetcher::Create(
      GURL(kRegistrationURL), net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_);

  std::string android_id = base::Uint64ToString(request_info_.android_id);
  std::string auth_header =
      std::string(net::HttpRequestHeaders::kAuthorization) + ": " +
      kLoginHeader + " " + android_id + ":" +
      base::Uint64ToString(request_info_.security_token);
  url_fetcher_->SetExtraRequestHeaders(auth_header);

  std::string body;
  BuildFormEncoding(kAppIdKey, request_info_.app_id, &body);
  BuildFormEncoding(kCertKey, request_info_.cert, &body);
  BuildFormEncoding(kDeviceIdKey, android_id, &body);

  std::string senders;
  for (std::vector<std::string>::const_iterator iter =
           request_info_.sender_ids.begin();
       iter != request_info_.sender_ids.end();
       ++iter) {
    DCHECK(!iter->empty());
    if (!senders.empty())
      senders.append(",");
    senders.append(*iter);
  }
  BuildFormEncoding(kSenderKey, senders, &body);

  DVLOG(1) << "Performing registration for: " << request_info_.app_id;
  DVLOG(1) << "Registration request: " << body;
  url_fetcher_->SetUploadData(kRegistrationRequestContentType, body);
  url_fetcher_->Start();
}

void RegistrationRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    url_fetcher_.reset();
    backoff_entry_.InformOfRequest(false);
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delaying GCM registration of app: "
             << request_info_.app_id << ", for "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RegistrationRequest::RetryWithBackoff,
                   weak_ptr_factory_.GetWeakPtr(),
                   false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

RegistrationRequest::Status RegistrationRequest::ParseResponse(
    const net::URLFetcher* source, std::string* token) {
  if (!source->GetStatus().is_success()) {
    LOG(ERROR) << "URL fetching failed.";
    return URL_FETCHING_FAILED;
  }
  if (source->GetResponseCode() != net::HTTP_OK) {
    LOG(ERROR) << "URL fetching HTTP response code is not OK. It is "
               << source->GetResponseCode();
    return HTTP_NOT_OK;
  }
  std::string response;
  if (!source->GetResponseAsString(&response)) {
    LOG(ERROR) << "Failed to parse registration response as a string.";
    return RESPONSE_PARSING_FAILED;
  }

  DVLOG(1) << "Parsing registration response: " << response;
  size_t token_pos = response.find(kTokenPrefix);
  if (token_pos != std::string::npos) {
    *token = response.substr(token_pos + arraysize(kTokenPrefix) - 1);
    return SUCCESS;
  }

  size_t error_pos = response.find(kErrorPrefix);
  if (error_pos == std::string::npos)
    return UNKNOWN_ERROR;
  std::string error = response.substr(error_pos + arraysize(kErrorPrefix) - 1);
  return GetStatusFromError(error);
}

void RegistrationRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string token;
  Status status = ParseResponse(source, &token);
  RecordRegistrationStatusToUMA(status );
  if (ShouldRetryWithStatus(status))
    RetryWithBackoff(true);
  else
    callback_.Run(status, token);
}

}  // namespace gcm
