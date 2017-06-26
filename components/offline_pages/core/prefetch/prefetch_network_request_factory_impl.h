// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_context_getter.h"

namespace offline_pages {
class GeneratePageBundleRequest;
class GetOperationRequest;

class PrefetchNetworkRequestFactoryImpl : public PrefetchNetworkRequestFactory {
 public:
  PrefetchNetworkRequestFactoryImpl(
      net::URLRequestContextGetter* request_context,
      version_info::Channel channel,
      const std::string& user_agent);

  ~PrefetchNetworkRequestFactoryImpl() override;

  void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      const PrefetchRequestFinishedCallback& callback) override;
  GeneratePageBundleRequest* CurrentGeneratePageBundleRequest() const override;

  void MakeGetOperationRequest(
      const std::string& operation_name,
      const PrefetchRequestFinishedCallback& callback) override;
  GetOperationRequest* FindGetOperationRequestByName(
      const std::string& operation_name) const override;

 private:
  void GeneratePageBundleRequestDone(
      const PrefetchRequestFinishedCallback& callback,
      PrefetchRequestStatus status,
      const std::string& operation_name,
      const std::vector<RenderPageInfo>& pages);
  void GetOperationRequestDone(const PrefetchRequestFinishedCallback& callback,
                               PrefetchRequestStatus status,
                               const std::string& operation_name,
                               const std::vector<RenderPageInfo>& pages);
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  version_info::Channel channel_;
  std::string user_agent_;

  std::unique_ptr<GeneratePageBundleRequest> generate_page_bundle_request_;
  std::map<std::string, std::unique_ptr<GetOperationRequest>>
      get_operation_requests_;

  base::WeakPtrFactory<PrefetchNetworkRequestFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNetworkRequestFactoryImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_IMPL_H_
