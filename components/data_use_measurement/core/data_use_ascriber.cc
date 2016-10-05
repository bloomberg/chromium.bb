// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_ascriber.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/data_use_measurement/core/data_use_network_delegate.h"
#include "components/data_use_measurement/core/data_use_recorder.h"

namespace data_use_measurement {

std::unique_ptr<net::NetworkDelegate> DataUseAscriber::CreateNetworkDelegate(
    std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate) {
  return base::MakeUnique<data_use_measurement::DataUseNetworkDelegate>(
      std::move(wrapped_network_delegate), this);
}

void DataUseAscriber::OnBeforeUrlRequest(net::URLRequest* request) {
  DataUseRecorder* recorder = GetDataUseRecorder(request);
  if (recorder)
    recorder->OnBeforeUrlRequest(request);
}

void DataUseAscriber::OnBeforeRedirect(net::URLRequest* request,
                                       const GURL& new_location) {}

void DataUseAscriber::OnNetworkBytesSent(net::URLRequest* request,
                                         int64_t bytes_sent) {
  DataUseRecorder* recorder = GetDataUseRecorder(request);
  if (recorder)
    recorder->OnNetworkBytesSent(request, bytes_sent);
}

void DataUseAscriber::OnNetworkBytesReceived(net::URLRequest* request,
                                             int64_t bytes_received) {
  DataUseRecorder* recorder = GetDataUseRecorder(request);
  if (recorder)
    recorder->OnNetworkBytesReceived(request, bytes_received);
}

void DataUseAscriber::OnUrlRequestCompleted(net::URLRequest* request,
                                            bool started) {
  DataUseRecorder* recorder = GetDataUseRecorder(request);
  // TODO(kundaji): Enforce DCHECK(recorder).
  if (recorder)
    recorder->OnUrlRequestCompleted(request);
}

}  // namespace data_use_measurement
