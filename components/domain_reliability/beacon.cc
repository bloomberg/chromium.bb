// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/beacon.h"

#include "base/values.h"
#include "net/base/net_errors.h"

namespace domain_reliability {

using base::Value;
using base::DictionaryValue;

DomainReliabilityBeacon::DomainReliabilityBeacon() {}
DomainReliabilityBeacon::~DomainReliabilityBeacon() {}

Value* DomainReliabilityBeacon::ToValue(base::TimeTicks upload_time) const {
  DictionaryValue* beacon_value = new DictionaryValue();
  if (!url.empty())
    beacon_value->SetString("url", url);
  if (!domain.empty())
    beacon_value->SetString("domain", domain);
  if (!resource.empty())
    beacon_value->SetString("resource", resource);
  beacon_value->SetString("status", status);
  if (chrome_error != net::OK) {
    DictionaryValue* failure_value = new DictionaryValue();
    failure_value->SetString("custom_error",
                             net::ErrorToString(chrome_error));
    beacon_value->Set("failure_data", failure_value);
  }
  beacon_value->SetString("server_ip", server_ip);
  beacon_value->SetString("protocol", protocol);
  if (http_response_code >= 0)
    beacon_value->SetInteger("http_response_code", http_response_code);
  beacon_value->SetInteger("request_elapsed_ms",
                           elapsed.InMilliseconds());
  beacon_value->SetInteger("request_age_ms",
                           (upload_time - start_time).InMilliseconds());
  return beacon_value;
}

}  // namespace domain_reliability
