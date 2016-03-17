// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/request_sender.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/update_client/configurator.h"
#include "components/update_client/utils.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"

namespace update_client {

namespace {

// This is an ECDSA prime256v1 named-curve key.
const int kKeyVersion = 5;
const char kKeyPubBytesBase64[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEB+Yi+3SdJKCyFJmm+suW3CyXygvVsbDbPnJgoC"
    "X4GeTtoL8Q/WjPx7CGtXOL1Xjbx0VPPN3DrvqZSL/oXy9hVw==";

}  // namespace

// This value is chosen not to conflict with network errors defined by
// net/base/net_error_list.h. The callers don't have to handle this error in
// any meaningful way, but this value may be reported in UMA stats, therefore
// avoiding collisions with known network errors is desirable.
int RequestSender::kErrorResponseNotTrusted = -10000;

RequestSender::RequestSender(const scoped_refptr<Configurator>& config)
    : config_(config), use_signing_(false) {}

RequestSender::~RequestSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void RequestSender::Send(bool use_signing,
                         const std::string& request_body,
                         const std::vector<GURL>& urls,
                         const RequestSenderCallback& request_sender_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  use_signing_ = use_signing;
  request_body_ = request_body;
  urls_ = urls;
  request_sender_callback_ = request_sender_callback;

  if (urls_.empty()) {
    return HandleSendError(-1);
  }

  cur_url_ = urls_.begin();

  if (use_signing_) {
    public_key_ = GetKey(kKeyPubBytesBase64);
    if (public_key_.empty())
      return HandleSendError(-1);
  }

  SendInternal();
}

void RequestSender::SendInternal() {
  DCHECK(cur_url_ != urls_.end());
  DCHECK(cur_url_->is_valid());
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL url(*cur_url_);

  if (use_signing_) {
    DCHECK(!public_key_.empty());
    signer_ = client_update_protocol::Ecdsa::Create(kKeyVersion, public_key_);
    std::string request_query_string;
    signer_->SignRequest(request_body_, &request_query_string);

    url = BuildUpdateUrl(url, request_query_string);
  }

  url_fetcher_ =
      SendProtocolRequest(url, request_body_, this, config_->RequestContext());
  if (!url_fetcher_.get())
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RequestSender::SendInternalComplete, base::Unretained(this),
                   -1, std::string(), std::string()));
}

void RequestSender::SendInternalComplete(int error,
                                         const std::string& response_body,
                                         const std::string& response_etag) {
  if (!error) {
    if (!use_signing_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(request_sender_callback_, 0, response_body));
      return;
    }

    DCHECK(use_signing_);
    DCHECK(signer_.get());
    if (signer_->ValidateResponse(response_body, response_etag)) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(request_sender_callback_, 0, response_body));
      return;
    }

    error = kErrorResponseNotTrusted;
  }

  DCHECK(error);
  if (++cur_url_ != urls_.end() &&
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&RequestSender::SendInternal, base::Unretained(this)))) {
    return;
  }

  HandleSendError(error);
}

void RequestSender::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(source);

  VLOG(1) << "request completed from url: " << source->GetOriginalURL().spec();

  const int fetch_error(GetFetchError(*source));
  std::string response_body;
  CHECK(source->GetResponseAsString(&response_body));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&RequestSender::SendInternalComplete, base::Unretained(this),
                 fetch_error, response_body, GetServerETag(source)));
}

void RequestSender::HandleSendError(int error) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(request_sender_callback_, error, std::string()));
}

std::string RequestSender::GetKey(const char* key_bytes_base64) {
  std::string result;
  return base::Base64Decode(std::string(key_bytes_base64), &result)
             ? result
             : std::string();
}

GURL RequestSender::BuildUpdateUrl(const GURL& url,
                                   const std::string& query_params) {
  const std::string query_string(
      url.has_query() ? base::StringPrintf("%s&%s", url.query().c_str(),
                                           query_params.c_str())
                      : query_params);
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);

  return url.ReplaceComponents(replacements);
}

std::string RequestSender::GetServerETag(const net::URLFetcher* source) {
  const auto response_headers(source->GetResponseHeaders());
  if (!response_headers)
    return std::string();

  std::string etag;
  return response_headers->EnumerateHeader(nullptr, "ETag", &etag)
             ? etag
             : std::string();
}

}  // namespace update_client
