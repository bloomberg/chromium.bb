// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

namespace {
const char* kUsedDataReductionProxyKey = "used_data_reduction_proxy";
const char* kLofiRequestedKey = "lofi_requested";
const char* kClientLofiRequestedKey = "client_lofi_requested";
const char* kLitePageReceivedKey = "lite_page_received";
const char* kLofiPolicyReceivedKey = "lofi_policy_received";
const char* kLofiReceivedKey = "lofi_received";
const char* kSessionKeyKey = "session_key";
const char* kEffectiveConnectionTypeKey = "effective_connection_type";
const char* kRequestURLKey = "request_url";
const char* kPageIdKey = "page_id";
}  // namespace

const void* const kDataReductionProxyUserDataKey =
    &kDataReductionProxyUserDataKey;

DataReductionProxyData::DataReductionProxyData()
    : used_data_reduction_proxy_(false),
      lofi_requested_(false),
      client_lofi_requested_(false),
      lite_page_received_(false),
      lofi_policy_received_(false),
      lofi_received_(false),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

DataReductionProxyData::~DataReductionProxyData() {}

// Convert from/to a base::Value.
base::Value DataReductionProxyData::ToValue() {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kUsedDataReductionProxyKey,
               base::Value(used_data_reduction_proxy_));
  value.SetKey(kLofiRequestedKey, base::Value(lofi_requested_));
  value.SetKey(kClientLofiRequestedKey, base::Value(client_lofi_requested_));
  value.SetKey(kLitePageReceivedKey, base::Value(lite_page_received_));
  value.SetKey(kLofiPolicyReceivedKey, base::Value(lofi_policy_received_));
  value.SetKey(kLofiReceivedKey, base::Value(lofi_received_));
  value.SetKey(kSessionKeyKey, base::Value(session_key_));
  value.SetKey(kEffectiveConnectionTypeKey,
               base::Value(effective_connection_type_));
  value.SetKey(kRequestURLKey,
               base::Value(request_url_.possibly_invalid_spec()));
  // Store |page_id_| as a string because base::Value can't store a 64 bits
  // integer.
  if (page_id_)
    value.SetKey(kPageIdKey, base::Value(std::to_string(page_id_.value())));

  return value;
}

DataReductionProxyData::DataReductionProxyData(const base::Value& value)
    : used_data_reduction_proxy_(
          value.FindKey(kUsedDataReductionProxyKey)->GetBool()),
      lofi_requested_(value.FindKey(kLofiRequestedKey)->GetBool()),
      client_lofi_requested_(value.FindKey(kClientLofiRequestedKey)->GetBool()),
      lite_page_received_(value.FindKey(kLitePageReceivedKey)->GetBool()),
      lofi_policy_received_(value.FindKey(kLofiPolicyReceivedKey)->GetBool()),
      lofi_received_(value.FindKey(kLofiReceivedKey)->GetBool()),
      session_key_(value.FindKey(kSessionKeyKey)->GetString()),
      request_url_(GURL(value.FindKey(kRequestURLKey)->GetString())),
      effective_connection_type_(net::EffectiveConnectionType(
          value.FindKey(kEffectiveConnectionTypeKey)->GetInt())) {
  const base::Value* page_id = value.FindKey(kPageIdKey);
  if (page_id)
    page_id_ = std::stoll(page_id->GetString());
}

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
