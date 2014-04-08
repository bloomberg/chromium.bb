// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/checkin_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace gcm {

namespace {
const char kCheckinURL[] = "https://android.clients.google.com/checkin";
const char kRequestContentType[] = "application/x-protobuf";
const int kRequestVersionValue = 2;
const int kDefaultUserSerialNumber = 0;

// This enum is also used in an UMA histogram (GCMCheckinRequestStatus
// enum defined in tools/metrics/histograms/histogram.xml). Hence the entries
// here shouldn't be deleted or re-ordered and new ones should be added to
// the end.
enum CheckinRequestStatus {
  SUCCESS,                    // Checkin completed successfully.
  URL_FETCHING_FAILED,        // URL fetching failed.
  HTTP_BAD_REQUEST,           // The request was malformed.
  HTTP_UNAUTHORIZED,          // The security token didn't match the android id.
  HTTP_NOT_OK,                // HTTP status was not OK.
  RESPONSE_PARSING_FAILED,    // Check in response parsing failed.
  ZERO_ID_OR_TOKEN,           // Either returned android id or security token
                              // was zero.
  // NOTE: always keep this entry at the end. Add new status types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  STATUS_COUNT
};

void RecordCheckinStatusToUMA(CheckinRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION("GCM.CheckinRequestStatus", status, STATUS_COUNT);
}

}  // namespace

CheckinRequest::CheckinRequest(
    const CheckinRequestCallback& callback,
    const net::BackoffEntry::Policy& backoff_policy,
    const checkin_proto::ChromeBuildProto& chrome_build_proto,
    uint64 android_id,
    uint64 security_token,
    const std::vector<std::string>& account_ids,
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter),
      callback_(callback),
      backoff_entry_(&backoff_policy),
      chrome_build_proto_(chrome_build_proto),
      android_id_(android_id),
      security_token_(security_token),
      account_ids_(account_ids),
      weak_ptr_factory_(this) {
}

CheckinRequest::~CheckinRequest() {}

void CheckinRequest::Start() {
  DCHECK(!url_fetcher_.get());

  checkin_proto::AndroidCheckinRequest request;
  request.set_id(android_id_);
  request.set_security_token(security_token_);
  request.set_user_serial_number(kDefaultUserSerialNumber);
  request.set_version(kRequestVersionValue);

  checkin_proto::AndroidCheckinProto* checkin = request.mutable_checkin();
  checkin->mutable_chrome_build()->CopyFrom(chrome_build_proto_);
#if defined(CHROME_OS)
  checkin->set_type(checkin_proto::DEVICE_CHROME_OS);
#else
  checkin->set_type(checkin_proto::DEVICE_CHROME_BROWSER);
#endif

  for (std::vector<std::string>::const_iterator iter = account_ids_.begin();
       iter != account_ids_.end();
       ++iter) {
    request.add_account_cookie("[" + *iter + "]");
  }

  std::string upload_data;
  CHECK(request.SerializeToString(&upload_data));

  url_fetcher_.reset(
      net::URLFetcher::Create(GURL(kCheckinURL), net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_);
  url_fetcher_->SetUploadData(kRequestContentType, upload_data);
  url_fetcher_->Start();
}

void CheckinRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    backoff_entry_.InformOfRequest(false);
    url_fetcher_.reset();
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delay GCM checkin for: "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CheckinRequest::RetryWithBackoff,
                   weak_ptr_factory_.GetWeakPtr(),
                   false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

void CheckinRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string response_string;
  checkin_proto::AndroidCheckinResponse response_proto;
  if (!source->GetStatus().is_success()) {
    LOG(ERROR) << "Failed to get checkin response. Fetcher failed. Retrying.";
    RecordCheckinStatusToUMA(URL_FETCHING_FAILED);
    RetryWithBackoff(true);
    return;
  }

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->GetResponseCode());
  if (response_status == net::HTTP_BAD_REQUEST ||
      response_status == net::HTTP_UNAUTHORIZED) {
    // BAD_REQUEST indicates that the request was malformed.
    // UNAUTHORIZED indicates that security token didn't match the android id.
    LOG(ERROR) << "No point retrying the checkin with status: "
               << response_status << ". Checkin failed.";
    RecordCheckinStatusToUMA(response_status == net::HTTP_BAD_REQUEST ?
        HTTP_BAD_REQUEST : HTTP_UNAUTHORIZED);
    callback_.Run(0,0);
    return;
  }

  if (response_status != net::HTTP_OK ||
      !source->GetResponseAsString(&response_string) ||
      !response_proto.ParseFromString(response_string)) {
    LOG(ERROR) << "Failed to get checkin response. HTTP Status: "
               << response_status << ". Retrying.";
    RecordCheckinStatusToUMA(response_status != net::HTTP_OK ?
        HTTP_NOT_OK : RESPONSE_PARSING_FAILED);
    RetryWithBackoff(true);
    return;
  }

  if (!response_proto.has_android_id() ||
      !response_proto.has_security_token() ||
      response_proto.android_id() == 0 ||
      response_proto.security_token() == 0) {
    LOG(ERROR) << "Android ID or security token is 0. Retrying.";
    RecordCheckinStatusToUMA(ZERO_ID_OR_TOKEN);
    RetryWithBackoff(true);
    return;
  }

  RecordCheckinStatusToUMA(SUCCESS);
  callback_.Run(response_proto.android_id(), response_proto.security_token());
}

}  // namespace gcm
