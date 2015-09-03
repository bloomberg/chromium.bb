// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_proof_fetcher.h"

#include <iterator>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/safe_json/safe_json_parser.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace certificate_transparency {

namespace {

// Shamelessly copied from domain_reliability/util.cc
int GetNetErrorFromURLRequestStatus(const net::URLRequestStatus& status) {
  switch (status.status()) {
    case net::URLRequestStatus::SUCCESS:
      return net::OK;
    case net::URLRequestStatus::CANCELED:
      return net::ERR_ABORTED;
    case net::URLRequestStatus::FAILED:
      return status.error();
    default:
      NOTREACHED();
      return net::ERR_FAILED;
  }
}

}  // namespace

struct LogProofFetcher::FetchState {
  FetchState(const std::string& log_id,
             const SignedTreeHeadFetchedCallback& fetched_callback,
             const FetchFailedCallback& failed_callback);
  ~FetchState();

  std::string log_id;
  SignedTreeHeadFetchedCallback fetched_callback;
  FetchFailedCallback failed_callback;
  scoped_refptr<net::IOBufferWithSize> response_buffer;
  std::string assembled_response;
};

LogProofFetcher::FetchState::FetchState(
    const std::string& log_id,
    const SignedTreeHeadFetchedCallback& fetched_callback,
    const FetchFailedCallback& failed_callback)
    : log_id(log_id),
      fetched_callback(fetched_callback),
      failed_callback(failed_callback),
      response_buffer(new net::IOBufferWithSize(kMaxLogResponseSizeInBytes)) {}

LogProofFetcher::FetchState::~FetchState() {}

LogProofFetcher::LogProofFetcher(net::URLRequestContext* request_context)
    : request_context_(request_context), weak_factory_(this) {
  DCHECK(request_context);
}

LogProofFetcher::~LogProofFetcher() {
  STLDeleteContainerPairPointers(inflight_requests_.begin(),
                                 inflight_requests_.end());
}

void LogProofFetcher::FetchSignedTreeHead(
    const GURL& base_log_url,
    const std::string& log_id,
    const SignedTreeHeadFetchedCallback& fetched_callback,
    const FetchFailedCallback& failed_callback) {
  DCHECK(base_log_url.SchemeIsHTTPOrHTTPS());
  GURL fetch_url(base_log_url.Resolve("ct/v1/get-sth"));
  scoped_ptr<net::URLRequest> request =
      request_context_->CreateRequest(fetch_url, net::DEFAULT_PRIORITY, this);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA);

  FetchState* fetch_state =
      new FetchState(log_id, fetched_callback, failed_callback);
  request->Start();
  inflight_requests_.insert(std::make_pair(request.release(), fetch_state));
}

void LogProofFetcher::OnResponseStarted(net::URLRequest* request) {
  net::URLRequestStatus status(request->status());
  DCHECK(inflight_requests_.count(request));
  FetchState* fetch_state = inflight_requests_.find(request)->second;

  if (!status.is_success() || request->GetResponseCode() != net::HTTP_OK) {
    int net_error = net::OK;
    int http_response_code = request->GetResponseCode();

    DVLOG(1) << "Fetching STH from " << request->original_url()
             << " failed. status:" << status.status()
             << " error:" << status.error()
             << " http response code: " << http_response_code;
    if (!status.is_success())
      net_error = GetNetErrorFromURLRequestStatus(status);

    InvokeFailureCallback(request, net_error, http_response_code);
    return;
  }

  StartNextRead(request, fetch_state);
}

void LogProofFetcher::OnReadCompleted(net::URLRequest* request,
                                      int bytes_read) {
  DCHECK(inflight_requests_.count(request));
  FetchState* fetch_state = inflight_requests_.find(request)->second;

  if (HandleReadResult(request, fetch_state, bytes_read))
    StartNextRead(request, fetch_state);
}

bool LogProofFetcher::HandleReadResult(net::URLRequest* request,
                                       FetchState* fetch_state,
                                       int bytes_read) {
  // Start by checking for an error condition.
  // If there are errors, invoke the failure callback and clean up the
  // request.
  if (bytes_read == -1 || !request->status().is_success()) {
    net::URLRequestStatus status(request->status());
    DVLOG(1) << "Read error: " << status.status() << " " << status.error();
    InvokeFailureCallback(request, GetNetErrorFromURLRequestStatus(status),
                          net::OK);

    return false;
  }

  // Not an error, but no data available, so wait for OnReadCompleted
  // callback.
  if (request->status().is_io_pending())
    return false;

  // Nothing more to read from the stream - finish handling the response.
  if (bytes_read == 0) {
    RequestComplete(request);
    return false;
  }

  // We have data, collect it and indicate another read is needed.
  DVLOG(1) << "Have " << bytes_read << " bytes to assemble.";
  DCHECK_GE(bytes_read, 0);
  fetch_state->assembled_response.append(fetch_state->response_buffer->data(),
                                         bytes_read);
  if (fetch_state->assembled_response.size() > kMaxLogResponseSizeInBytes) {
    // Log response is too big, invoke the failure callback.
    InvokeFailureCallback(request, net::ERR_FILE_TOO_BIG, net::HTTP_OK);
    return false;
  }

  return true;
}

void LogProofFetcher::StartNextRead(net::URLRequest* request,
                                    FetchState* fetch_state) {
  bool continue_reading = true;
  while (continue_reading) {
    int read_bytes = 0;
    request->Read(fetch_state->response_buffer.get(),
                  fetch_state->response_buffer->size(), &read_bytes);
    continue_reading = HandleReadResult(request, fetch_state, read_bytes);
  }
}

void LogProofFetcher::RequestComplete(net::URLRequest* request) {
  DCHECK(inflight_requests_.count(request));

  FetchState* fetch_state = inflight_requests_.find(request)->second;

  // Get rid of the buffer as it really isn't necessary.
  fetch_state->response_buffer = nullptr;
  safe_json::SafeJsonParser::Parse(
      fetch_state->assembled_response,
      base::Bind(&LogProofFetcher::OnSTHJsonParseSuccess,
                 weak_factory_.GetWeakPtr(), request),
      base::Bind(&LogProofFetcher::OnSTHJsonParseError,
                 weak_factory_.GetWeakPtr(), request));
}

void LogProofFetcher::CleanupRequest(net::URLRequest* request) {
  DVLOG(1) << "Cleaning up request to " << request->original_url();
  auto it = inflight_requests_.find(request);
  DCHECK(it != inflight_requests_.end());
  auto next_it = it;
  std::advance(next_it, 1);

  // Delete FetchState and URLRequest, then the entry from inflight_requests_.
  STLDeleteContainerPairPointers(it, next_it);
  inflight_requests_.erase(it);
}

void LogProofFetcher::InvokeFailureCallback(net::URLRequest* request,
                                            int net_error,
                                            int http_response_code) {
  DCHECK(inflight_requests_.count(request));
  auto it = inflight_requests_.find(request);
  FetchState* fetch_state = it->second;

  fetch_state->failed_callback.Run(fetch_state->log_id, net_error,
                                   http_response_code);
  CleanupRequest(request);
}

void LogProofFetcher::OnSTHJsonParseSuccess(
    net::URLRequest* request,
    scoped_ptr<base::Value> parsed_json) {
  DCHECK(inflight_requests_.count(request));

  FetchState* fetch_state = inflight_requests_.find(request)->second;
  net::ct::SignedTreeHead signed_tree_head;
  if (net::ct::FillSignedTreeHead(*parsed_json.get(), &signed_tree_head)) {
    fetch_state->fetched_callback.Run(fetch_state->log_id, signed_tree_head);
  } else {
    fetch_state->failed_callback.Run(fetch_state->log_id,
                                     net::ERR_CT_STH_INCOMPLETE, net::HTTP_OK);
  }

  CleanupRequest(request);
}

void LogProofFetcher::OnSTHJsonParseError(net::URLRequest* request,
                                          const std::string& error) {
  InvokeFailureCallback(request, net::ERR_CT_STH_PARSING_FAILED, net::HTTP_OK);
}

}  // namespace certificate_transparency
