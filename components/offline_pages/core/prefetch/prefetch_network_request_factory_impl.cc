// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"

constexpr int kMaxBundleSizeBytes = 20 * 1024 * 1024;  // 20 MB

namespace offline_pages {

PrefetchNetworkRequestFactoryImpl::PrefetchNetworkRequestFactoryImpl(
    net::URLRequestContextGetter* request_context,
    version_info::Channel channel,
    const std::string& user_agent)
    : request_context_(request_context),
      channel_(channel),
      user_agent_(user_agent),
      weak_factory_(this) {}

PrefetchNetworkRequestFactoryImpl::~PrefetchNetworkRequestFactoryImpl() =
    default;

void PrefetchNetworkRequestFactoryImpl::MakeGeneratePageBundleRequest(
    const std::vector<std::string>& url_strings,
    const std::string& gcm_registration_id,
    const PrefetchRequestFinishedCallback& callback) {
  generate_page_bundle_request_ = base::MakeUnique<GeneratePageBundleRequest>(
      user_agent_, gcm_registration_id, kMaxBundleSizeBytes, url_strings,
      channel_, request_context_.get(),
      base::Bind(
          &PrefetchNetworkRequestFactoryImpl::GeneratePageBundleRequestDone,
          weak_factory_.GetWeakPtr(), callback));
}

GeneratePageBundleRequest*
PrefetchNetworkRequestFactoryImpl::CurrentGeneratePageBundleRequest() const {
  return generate_page_bundle_request_.get();
}

void PrefetchNetworkRequestFactoryImpl::MakeGetOperationRequest(
    const std::string& operation_name,
    const PrefetchRequestFinishedCallback& callback) {
  get_operation_requests_[operation_name] =
      base::MakeUnique<GetOperationRequest>(
          operation_name, channel_, request_context_.get(),
          base::Bind(
              &PrefetchNetworkRequestFactoryImpl::GetOperationRequestDone,
              weak_factory_.GetWeakPtr(), callback));
}

void PrefetchNetworkRequestFactoryImpl::GeneratePageBundleRequestDone(
    const PrefetchRequestFinishedCallback& callback,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  callback.Run(status, operation_name, pages);
  generate_page_bundle_request_.reset();
}

void PrefetchNetworkRequestFactoryImpl::GetOperationRequestDone(
    const PrefetchRequestFinishedCallback& callback,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  callback.Run(status, operation_name, pages);
  get_operation_requests_.erase(operation_name);
}

GetOperationRequest*
PrefetchNetworkRequestFactoryImpl::FindGetOperationRequestByName(
    const std::string& operation_name) const {
  auto iter = get_operation_requests_.find(operation_name);
  if (iter == get_operation_requests_.end())
    return nullptr;

  return iter->second.get();
}

}  // namespace offline_pages
