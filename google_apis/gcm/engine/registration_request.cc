// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/registration_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
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
const char kUserAndroidIdKey[] = "X-GOOG.USER_AID";
const char kUserSerialNumberKey[] = "device_user_id";

// Request validation constants.
const size_t kMaxSenders = 100;

// Response constants.
const char kTokenPrefix[] = "token=";

void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + net::EscapeUrlEncodedData(value, true));
}

}  // namespace

RegistrationRequest::RequestInfo::RequestInfo(
    uint64 android_id,
    uint64 security_token,
    uint64 user_android_id,
    int64 user_serial_number,
    const std::string& app_id,
    const std::string& cert,
    const std::vector<std::string>& sender_ids)
    : android_id(android_id),
      security_token(security_token),
      user_android_id(user_android_id),
      user_serial_number(user_serial_number),
      app_id(app_id),
      cert(cert),
      sender_ids(sender_ids) {}

RegistrationRequest::RequestInfo::~RequestInfo() {}

RegistrationRequest::RegistrationRequest(
    const RequestInfo& request_info,
    const RegistrationCallback& callback,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : callback_(callback),
      request_info_(request_info),
      request_context_getter_(request_context_getter) {}

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

  if (request_info_.user_serial_number != 0) {
    DCHECK(request_info_.user_android_id != 0);
    BuildFormEncoding(kUserSerialNumberKey,
                      base::Int64ToString(request_info_.user_serial_number),
                      &body);
    BuildFormEncoding(kUserAndroidIdKey,
                      base::Uint64ToString(request_info_.user_android_id),
                      &body);
  }

  DVLOG(1) << "Registration request: " << body;
  url_fetcher_->SetUploadData(kRegistrationRequestContentType, body);

  DVLOG(1) << "Performing registration for: " << request_info_.app_id;
  url_fetcher_->Start();
}

void RegistrationRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string response;
  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK ||
      !source->GetResponseAsString(&response)) {
    // TODO(fgoski): Introduce retry logic.
    // TODO(jianli): Handle "Error=INVALID_SENDER".
    LOG(ERROR) << "Failed to get registration response: "
               << source->GetStatus().is_success() << " "
               << source->GetResponseCode();
    callback_.Run("");
    return;
  }

  DVLOG(1) << "Parsing registration response: " << response;
  size_t token_pos = response.find(kTokenPrefix);
  std::string token;
  if (token_pos != std::string::npos)
    token = response.substr(token_pos + strlen(kTokenPrefix));
  else
    LOG(ERROR) << "Failed to extract token.";
  callback_.Run(token);
}

}  // namespace gcm
