// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_reporter.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"

namespace content {

// static
std::unique_ptr<SignedExchangeReporter> SignedExchangeReporter::MaybeCreate(
    const GURL& outer_url,
    const std::string& referrer,
    const network::ResourceResponseHead& response,
    base::OnceCallback<int(void)> frame_tree_node_id_getter) {
  if (!base::FeatureList::IsEnabled(network::features::kReporting) ||
      !base::FeatureList::IsEnabled(
          features::kSignedExchangeReportingForDistributors)) {
    return nullptr;
  }
  return base::WrapUnique(new SignedExchangeReporter(
      outer_url, referrer, response, std::move(frame_tree_node_id_getter)));
}

SignedExchangeReporter::SignedExchangeReporter(
    const GURL& outer_url,
    const std::string& referrer,
    const network::ResourceResponseHead& response,
    base::OnceCallback<int(void)> frame_tree_node_id_getter)
    : outer_url_(outer_url),
      referrer_(referrer),
      server_ip_(response.socket_address.host()),
      status_code_(response.headers ? response.headers->response_code() : 0),
      frame_tree_node_id_getter_(std::move(frame_tree_node_id_getter)) {}

SignedExchangeReporter::~SignedExchangeReporter() = default;

void SignedExchangeReporter::set_cert_server_ip(
    const std::string& cert_server_ip) {
  cert_server_ip_ = cert_server_ip;
}

void SignedExchangeReporter::set_inner_url(const GURL& inner_url) {
  inner_url_ = inner_url;
}

void SignedExchangeReporter::set_cert_url(const GURL& cert_url) {
  cert_url_ = cert_url;
}

void SignedExchangeReporter::ReportResult(SignedExchangeLoadResult result) {
  // TODO(910516):Implement this. |frame_tree_node_id_getter_| will be used
  // to get mojom::NetworkContext and call a method to queue the report.
}

}  // namespace content
