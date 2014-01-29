// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/checkin_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
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
}  // namespace

CheckinRequest::CheckinRequest(
    const CheckinRequestCallback& callback,
    const checkin_proto::ChromeBuildProto& chrome_build_proto,
    int64 user_serial_number,
    uint64 android_id,
    uint64 security_token,
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter),
      callback_(callback),
      chrome_build_proto_(chrome_build_proto),
      android_id_(android_id),
      security_token_(security_token),
      user_serial_number_(user_serial_number) {}

CheckinRequest::~CheckinRequest() {}

void CheckinRequest::Start() {
  DCHECK(!url_fetcher_.get());

  checkin_proto::AndroidCheckinRequest request;
  request.set_id(android_id_);
  request.set_security_token(security_token_);
  request.set_user_serial_number(user_serial_number_);
  request.set_version(kRequestVersionValue);

  checkin_proto::AndroidCheckinProto* checkin = request.mutable_checkin();
  checkin->mutable_chrome_build()->CopyFrom(chrome_build_proto_);
#if defined(CHROME_OS)
  checkin->set_type(checkin_proto::DEVICE_CHROME_OS);
#else
  checkin->set_type(checkin_proto::DEVICE_CHROME_BROWSER);
#endif


  std::string upload_data;
  CHECK(request.SerializeToString(&upload_data));

  url_fetcher_.reset(
      net::URLFetcher::Create(GURL(kCheckinURL), net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_);
  url_fetcher_->SetUploadData(kRequestContentType, upload_data);
  url_fetcher_->Start();
}

void CheckinRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string response_string;
  checkin_proto::AndroidCheckinResponse response_proto;
  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK ||
      !source->GetResponseAsString(&response_string) ||
      !response_proto.ParseFromString(response_string)) {
    LOG(ERROR) << "Failed to get checkin response: "
               << source->GetStatus().is_success() << " "
               << source->GetResponseCode();
    // TODO(fgorski): Handle retry logic for certain responses.
    callback_.Run(0, 0);
    return;
  }

  if (!response_proto.has_android_id() ||
      !response_proto.has_security_token() ||
      response_proto.android_id() == 0 ||
      response_proto.security_token() == 0) {
    LOG(ERROR) << "Badly formatted checkin response.";
    callback_.Run(0, 0);
    return;
  }

  callback_.Run(response_proto.android_id(), response_proto.security_token());
}

}  // namespace gcm
