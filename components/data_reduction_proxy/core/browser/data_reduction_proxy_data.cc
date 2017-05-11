// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"

#include "base/memory/ptr_util.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

const void* const kDataReductionProxyUserDataKey =
    &kDataReductionProxyUserDataKey;

DataReductionProxyData::DataReductionProxyData()
    : used_data_reduction_proxy_(false),
      lofi_requested_(false),
      client_lofi_requested_(false),
      lite_page_received_(false),
      lofi_received_(false),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

DataReductionProxyData::~DataReductionProxyData() {}

std::unique_ptr<DataReductionProxyData> DataReductionProxyData::DeepCopy()
    const {
  std::unique_ptr<DataReductionProxyData> copy(new DataReductionProxyData());
  copy->used_data_reduction_proxy_ = used_data_reduction_proxy_;
  copy->lofi_requested_ = lofi_requested_;
  copy->client_lofi_requested_ = client_lofi_requested_;
  copy->lite_page_received_ = lite_page_received_;
  copy->lofi_received_ = lofi_received_;
  copy->session_key_ = session_key_;
  copy->request_url_ = request_url_;
  copy->effective_connection_type_ = effective_connection_type_;
  copy->page_id_ = page_id_;
  return copy;
}

DataReductionProxyData* DataReductionProxyData::GetData(
    const net::URLRequest& request) {
  DataReductionProxyData* data = static_cast<DataReductionProxyData*>(
      request.GetUserData(kDataReductionProxyUserDataKey));
  return data;
}

DataReductionProxyData* DataReductionProxyData::GetDataAndCreateIfNecessary(
    net::URLRequest* request) {
  if (!request)
    return nullptr;
  DataReductionProxyData* data = GetData(*request);
  if (data)
    return data;
  data = new DataReductionProxyData();
  request->SetUserData(kDataReductionProxyUserDataKey, base::WrapUnique(data));
  return data;
}

void DataReductionProxyData::ClearData(net::URLRequest* request) {
  request->RemoveUserData(kDataReductionProxyUserDataKey);
}

}  // namespace data_reduction_proxy
