// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_error_logging/network_error_logging_service.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "net/reporting/reporting_service.h"
#include "url/origin.h"

namespace features {

const base::Feature kNetworkErrorLogging{"NetworkErrorLogging",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace network_error_logging {

// static
std::unique_ptr<NetworkErrorLoggingService>
NetworkErrorLoggingService::Create() {
  if (!base::FeatureList::IsEnabled(features::kNetworkErrorLogging))
    return std::unique_ptr<NetworkErrorLoggingService>();

  // Would be MakeUnique, but the constructor is private so MakeUnique can't see
  // it.
  return base::WrapUnique(new NetworkErrorLoggingService());
}

NetworkErrorLoggingService::~NetworkErrorLoggingService() {}

void NetworkErrorLoggingService::SetReportingService(
    net::ReportingService* reporting_service) {
  reporting_service_ = reporting_service;
}

void NetworkErrorLoggingService::OnHeader(const url::Origin& origin,
                                          const std::string& value) {}

void NetworkErrorLoggingService::OnNetworkError(
    const url::Origin& origin,
    net::Error error,
    ErrorDetailsCallback details_callback) {}

NetworkErrorLoggingService::NetworkErrorLoggingService()
    : reporting_service_(nullptr) {}

}  // namespace network_error_logging
