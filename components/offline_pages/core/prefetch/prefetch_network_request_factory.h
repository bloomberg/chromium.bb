// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {
class GeneratePageBundleRequest;
class GetOperationRequest;

class PrefetchNetworkRequestFactory {
 public:
  virtual ~PrefetchNetworkRequestFactory() = default;

  // Creates and starts a new GeneratePageBundle request, retaining ownership.
  // If a GeneratePageBundle request already exists, this will cancel the
  // existing request and start a new one.
  virtual void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      const PrefetchRequestFinishedCallback& callback) = 0;
  // Gets the current GeneratePageBundleRequest.  After |callback| is executed,
  // this will return |nullptr|.
  virtual GeneratePageBundleRequest* CurrentGeneratePageBundleRequest()
      const = 0;

  // Creates and starts a new GetOperationRequest, retaining ownership.
  // If a GetOperation request already exists with the same operation name, this
  // will cancel the existing request and start a new one.
  virtual void MakeGetOperationRequest(
      const std::string& operation_name,
      const PrefetchRequestFinishedCallback& callback) = 0;
  // Returns the current GetOperationRequest with the given name, if any.
  virtual GetOperationRequest* FindGetOperationRequestByName(
      const std::string& operation_name) const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_
