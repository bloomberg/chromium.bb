// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_channel_status_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/gcm_driver/gcm_backoff_policy.h"
#include "components/gcm_driver/proto/gcm_channel_status.pb.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace gcm {

namespace {

const char kGCMChannelStatusRequestURL[] =
    "https://clients4.google.com/chrome-sync/command/";
const char kRequestContentType[] = "application/octet-stream";
const char kGCMChannelTag[] = "gcm_channel";
const int kDefaultPollIntervalSeconds = 60 * 60;  // 60 minutes.
const int kMinPollIntervalSeconds = 30 * 60;  // 30 minutes.

}  // namespace

GCMChannelStatusRequest::GCMChannelStatusRequest(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const GCMChannelStatusRequestCallback& callback)
    : request_context_getter_(request_context_getter),
      callback_(callback),
      backoff_entry_(&(GetGCMBackoffPolicy())),
      weak_ptr_factory_(this) {
}

GCMChannelStatusRequest::~GCMChannelStatusRequest() {
}

// static
int GCMChannelStatusRequest::default_poll_interval_seconds() {
  return kDefaultPollIntervalSeconds;
}

// static
int GCMChannelStatusRequest::min_poll_interval_seconds() {
  return kMinPollIntervalSeconds;
}

void GCMChannelStatusRequest::Start() {
  DCHECK(!url_fetcher_.get());

  GURL request_url(kGCMChannelStatusRequestURL);

  gcm_proto::ExperimentStatusRequest proto_data;
  proto_data.add_experiment_name(kGCMChannelTag);
  std::string upload_data;
  DCHECK(proto_data.SerializeToString(&upload_data));

  url_fetcher_.reset(
      net::URLFetcher::Create(request_url, net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_getter_.get());
  url_fetcher_->SetUploadData(kRequestContentType, upload_data);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->Start();
}

void GCMChannelStatusRequest::OnURLFetchComplete(
    const net::URLFetcher* source) {
  if (ParseResponse(source))
    return;

  RetryWithBackoff(true);
}

bool GCMChannelStatusRequest::ParseResponse(const net::URLFetcher* source) {
  if (!source->GetStatus().is_success()) {
    LOG(ERROR) << "GCM channel request failed.";
    return false;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    LOG(ERROR) << "GCM channel request failed. HTTP status: "
               << source->GetResponseCode();
    return false;
  }

  std::string response_string;
  if (!source->GetResponseAsString(&response_string) ||
      response_string.empty()) {
    LOG(ERROR) << "GCM channel response failed to be retrieved.";
    return false;
  }

  gcm_proto::ExperimentStatusResponse response_proto;
  if (!response_proto.ParseFromString(response_string)) {
    LOG(ERROR) << "GCM channel response failed to be parse as proto.";
    return false;
  }

  bool enabled = true;
  if (response_proto.has_gcm_channel() &&
      response_proto.gcm_channel().has_enabled()) {
    enabled = response_proto.gcm_channel().enabled();
  }

  int poll_interval_seconds;
  if (response_proto.has_poll_interval_seconds())
    poll_interval_seconds = response_proto.poll_interval_seconds();
  else
    poll_interval_seconds = kDefaultPollIntervalSeconds;
  if (poll_interval_seconds < kMinPollIntervalSeconds)
    poll_interval_seconds = kMinPollIntervalSeconds;

  callback_.Run(enabled, poll_interval_seconds);

  return true;
}

void GCMChannelStatusRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    url_fetcher_.reset();
    backoff_entry_.InformOfRequest(false);
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delaying GCM channel request for "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " ms.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GCMChannelStatusRequest::RetryWithBackoff,
                   weak_ptr_factory_.GetWeakPtr(),
                   false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

}  // namespace gcm
