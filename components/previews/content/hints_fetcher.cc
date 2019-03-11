// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hints_fetcher.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/hint_cache.h"
#include "components/previews/core/previews_experiments.h"

namespace previews {

HintsFetcher::HintsFetcher(HintCache* hint_cache) : hint_cache_(hint_cache) {}

HintsFetcher::~HintsFetcher() {}

void HintsFetcher::FetchHintsForHosts(const std::vector<std::string>& hosts) {
  SEQUENCE_CHECKER(sequence_checker_);

  UMA_HISTOGRAM_COUNTS_100("Previews.HintsFetcher.GetHintsRequest.HostCount",
                           hosts.size());
  GetRemoteHints(hosts);
}

void HintsFetcher::GetRemoteHints(const std::vector<std::string>& hosts) {
  get_hints_request_ =
      std::make_unique<optimization_guide::proto::GetHintsRequest>();

  // Add all the optimizations supported by the current version of Chrome,
  // regardless of whether the session is in holdback for either of them.
  get_hints_request_->add_supported_optimizations(
      optimization_guide::proto::NOSCRIPT);
  get_hints_request_->add_supported_optimizations(
      optimization_guide::proto::RESOURCE_LOADING);
  static_assert(static_cast<int>(PreviewsType::LITE_PAGE_REDIRECT) + 1 ==
                    static_cast<int>(PreviewsType::LAST),
                "PreviewsType has been updated, make sure OnePlatform hints "
                "are not affected");

  get_hints_request_->set_context(
      optimization_guide::proto::CONTEXT_BATCH_UPDATE);

  for (const auto& host : hosts) {
    optimization_guide::proto::HostInfo* host_info =
        get_hints_request_->add_hosts();
    host_info->set_host(host);
  }
}

void HintsFetcher::HandleResponse(const std::string& config_data,
                                  int status,
                                  int response_code) {}

void HintsFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {}

bool HintsFetcher::ParseGetHintsResponseAndApplyHints(
    const optimization_guide::proto::GetHintsResponse& get_hints_response) {
  ALLOW_UNUSED_LOCAL(hint_cache_);
  return false;
}

}  // namespace previews
