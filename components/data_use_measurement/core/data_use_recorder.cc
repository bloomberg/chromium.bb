// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_recorder.h"

#include "net/url_request/url_request.h"

namespace data_use_measurement {

DataUseRecorder::DataUseRecorder(DataUse::TrafficType traffic_type)
    : main_url_request_(nullptr), data_use_(traffic_type), is_visible_(false) {}

DataUseRecorder::~DataUseRecorder() {}

bool DataUseRecorder::IsDataUseComplete() {
  return pending_url_requests_.empty();
}

void DataUseRecorder::AddPendingURLRequest(net::URLRequest* request) {
  pending_url_requests_.emplace(std::piecewise_construct,
                                std::forward_as_tuple(request),
                                std::forward_as_tuple());
}

void DataUseRecorder::OnUrlRequestDestroyed(net::URLRequest* request) {
  pending_url_requests_.erase(request);
}

void DataUseRecorder::MovePendingURLRequest(DataUseRecorder* other,
                                            net::URLRequest* request) {
  auto request_it = pending_url_requests_.find(request);
  DCHECK(request_it != pending_url_requests_.end());
  DCHECK(other->pending_url_requests_.find(request) ==
         other->pending_url_requests_.end());

  // Increment the bytes of the request in |other|, and decrement the bytes in
  // |this|.
  // TODO(rajendrant): Check if the moving the bytes in |data_use_| needs to be
  // propogated to observers, which could store per-request user data.
  other->AddPendingURLRequest(request);
  other->UpdateNetworkByteCounts(request, request_it->second.bytes_received,
                                 request_it->second.bytes_sent);
  data_use_.IncrementTotalBytes(-request_it->second.bytes_received,
                                -request_it->second.bytes_sent);
  pending_url_requests_.erase(request_it);
}

void DataUseRecorder::RemoveAllPendingURLRequests() {
  pending_url_requests_.clear();
}

void DataUseRecorder::OnBeforeUrlRequest(net::URLRequest* request) {}

void DataUseRecorder::OnNetworkBytesReceived(net::URLRequest* request,
                                             int64_t bytes_received) {
  UpdateNetworkByteCounts(request, bytes_received, 0);
}

void DataUseRecorder::OnNetworkBytesSent(net::URLRequest* request,
                                         int64_t bytes_sent) {
  UpdateNetworkByteCounts(request, 0, bytes_sent);
}

void DataUseRecorder::UpdateNetworkByteCounts(net::URLRequest* request,
                                              int64_t bytes_received,
                                              int64_t bytes_sent) {
  data_use_.IncrementTotalBytes(bytes_received, bytes_sent);
  auto request_it = pending_url_requests_.find(request);
  request_it->second.bytes_received += bytes_received;
  request_it->second.bytes_sent += bytes_sent;
}

}  // namespace data_use_measurement
