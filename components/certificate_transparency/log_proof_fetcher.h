// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class URLRequestContext;

namespace ct {
struct SignedTreeHead;
}  // namespace ct

}  // namespace net

class GURL;

namespace certificate_transparency {

// Fetches Signed Tree Heads (STHs) and consistency proofs from Certificate
// Transparency logs using the URLRequestContext provided during the instance
// construction.
// Must outlive the provided URLRequestContext.
class LogProofFetcher : public net::URLRequest::Delegate {
 public:
  static const size_t kMaxLogResponseSizeInBytes = 600;

  // Callback for successful retrieval of Signed Tree Heads. Called
  // with the log_id of the log the STH belogs to (as supplied by the caller
  // to FetchSignedTreeHead) and the STH itself.
  using SignedTreeHeadFetchedCallback =
      base::Callback<void(const std::string& log_id,
                          const net::ct::SignedTreeHead& signed_tree_head)>;

  // Callback for failure of Signed Tree Head retrieval. Called with the log_id
  // that the log fetching was requested for and a net error code of the
  // failure.
  using FetchFailedCallback = base::Callback<
      void(const std::string& log_id, int net_error, int http_response_code)>;

  explicit LogProofFetcher(net::URLRequestContext* request_context);
  ~LogProofFetcher() override;

  // Fetch the latest Signed Tree Head from the log identified by |log_id|
  // from |base_log_url|. The |log_id| will be passed into the callbacks to
  // identify the log the retrieved Signed Tree Head belongs to.
  // The callbacks won't be invoked if the request is destroyed before
  // fetching is completed.
  // It is possible, but does not make a lot of sense, to have multiple
  // Signed Tree Head fetching requests going out to the same log, since
  // they are likely to return the same result.
  // TODO(eranm): Think further about whether multiple requests to the same
  // log imply cancellation of previous requests, should be coalesced or handled
  // independently.
  void FetchSignedTreeHead(
      const GURL& base_log_url,
      const std::string& log_id,
      const SignedTreeHeadFetchedCallback& fetched_callback,
      const FetchFailedCallback& failed_callback);

  // net::URLRequest::Delegate
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

 private:
  struct FetchState;
  // Handles the final result of a URLRequest::Read call on |request|.
  // Returns true if another read should be started, false if the read
  // failed completely or we have to wait for OnResponseStarted to
  // be called.
  bool HandleReadResult(net::URLRequest* request,
                        FetchState* params,
                        int bytes_read);

  // Calls URLRequest::Read on |request| repeatedly, until HandleReadResult
  // indicates it should no longer be called. Usually this would be when there
  // is pending IO that requires waiting for OnResponseStarted to be called.
  void StartNextRead(net::URLRequest* request, FetchState* params);

  // Performs post-report cleanup.
  void RequestComplete(net::URLRequest* request);
  // Deletes the request and associated FetchState from the internal map.
  void CleanupRequest(net::URLRequest* request);
  // Invokes the failure callback with the supplied arguments, then cleans up
  // the request.
  void InvokeFailureCallback(net::URLRequest* request,
                             int net_error,
                             int http_response_code);

  // Callbacks for parsing the STH's JSON by the SafeJsonParser
  void OnSTHJsonParseSuccess(net::URLRequest* request,
                             scoped_ptr<base::Value> parsed_json);
  void OnSTHJsonParseError(net::URLRequest* request, const std::string& error);

  net::URLRequestContext* const request_context_;

  // Owns the contained requests, as well as FetchState.
  std::map<net::URLRequest*, FetchState*> inflight_requests_;

  base::WeakPtrFactory<LogProofFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogProofFetcher);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_
