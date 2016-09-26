// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_network_delegate.h"

#include <memory>
#include <utility>

#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "net/url_request/url_request.h"

namespace data_use_measurement {
DataUseNetworkDelegate::DataUseNetworkDelegate(
    std::unique_ptr<NetworkDelegate> nested_network_delegate,
    DataUseAscriber* ascriber)
    : net::LayeredNetworkDelegate(std::move(nested_network_delegate)),
      ascriber_(ascriber) {
  DCHECK(ascriber);
}

DataUseNetworkDelegate::~DataUseNetworkDelegate() {}

void DataUseNetworkDelegate::OnBeforeURLRequestInternal(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  ascriber_->OnBeforeUrlRequest(request);
}

void DataUseNetworkDelegate::OnBeforeRedirectInternal(
    net::URLRequest* request,
    const GURL& new_location) {
  ascriber_->OnBeforeRedirect(request, new_location);
}

void DataUseNetworkDelegate::OnNetworkBytesReceivedInternal(
    net::URLRequest* request,
    int64_t bytes_received) {
  ascriber_->OnNetworkBytesReceived(request, bytes_received);
}

void DataUseNetworkDelegate::OnNetworkBytesSentInternal(
    net::URLRequest* request,
    int64_t bytes_sent) {
  ascriber_->OnNetworkBytesSent(request, bytes_sent);
}

void DataUseNetworkDelegate::OnCompletedInternal(net::URLRequest* request,
                                                 bool started) {
  ascriber_->OnUrlRequestCompleted(request, started);
}

}  // namespace data_use_measurement
