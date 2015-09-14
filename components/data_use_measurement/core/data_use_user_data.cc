// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_user_data.h"

#include "net/url_request/url_fetcher.h"

namespace data_use_measurement {

DataUseUserData::DataUseUserData(ServiceName service_name)
    : service_name_(service_name) {}

DataUseUserData::~DataUseUserData() {}

// static
const void* DataUseUserData::kUserDataKey =
    static_cast<const void*>(&DataUseUserData::kUserDataKey);

// static
base::SupportsUserData::Data* DataUseUserData::Create(
    ServiceName service_name) {
  return new DataUseUserData(service_name);
}

// static
std::string DataUseUserData::GetServiceNameAsString(ServiceName service_name) {
  switch (service_name) {
    case SUGGESTIONS:
      return "Suggestions";
    case NOT_TAGGED:
      return "NotTagged";
  }
  return "INVALID";
}

// static
void DataUseUserData::AttachToFetcher(net::URLFetcher* fetcher,
                                      ServiceName service_name) {
  fetcher->SetURLRequestUserData(kUserDataKey,
                                 base::Bind(&Create, service_name));
}

}  // namespace data_use_measurement
