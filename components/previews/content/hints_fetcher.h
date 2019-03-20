// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace previews {

class HintCache;

// A class to handle requests for optimization hints from a remote Optimization
// Guide Service.
//
// This class fetches new hints from the remote Optimization Guide Service.
// Owner must ensure that |hint_cache| remains alive for the lifetime of
// |HintsFetcher|.
class HintsFetcher {
 public:
  HintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      HintCache* hint_cache);
  ~HintsFetcher();

  // Requests hints from the Optimization Guide Service if a request for
  // them is not already in progress. Returns whether a new request was
  // issued.
  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);


  // Parses the hints component of |get_hints_response| and applies it to
  // |hints_|. Returns true if |get_hints_response| was successfully
  // parsed and applied.
  bool ParseGetHintsResponseAndApplyHints(
      const optimization_guide::proto::GetHintsResponse& get_hints_response);

  // Used to hold the GetHintsRequest being constructed and sent as a remote
  // request.
  std::unique_ptr<optimization_guide::proto::GetHintsRequest>
      get_hints_request_;

  // HintCache (unowned) is a reference to the set of Optimization Hints
  // provided by the remote Optimization Guide Service. The caller must ensure
  // that the |hints_cache_| outlives this instance.
  HintCache* hint_cache_ = nullptr;

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // Holds the |URLLoader| for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for creating a |url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintsFetcher);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINTS_FETCHER_H_
