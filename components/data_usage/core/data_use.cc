// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use.h"

namespace data_usage {

DataUse::DataUse(const GURL& url,
                 const base::TimeTicks& request_start,
                 const GURL& first_party_for_cookies,
                 int32_t tab_id,
                 net::NetworkChangeNotifier::ConnectionType connection_type,
                 int64_t tx_bytes,
                 int64_t rx_bytes)
    : url(url),
      request_start(request_start),
      first_party_for_cookies(first_party_for_cookies),
      tab_id(tab_id),
      connection_type(connection_type),
      tx_bytes(tx_bytes),
      rx_bytes(rx_bytes) {}

bool DataUse::CanCombineWith(const DataUse& other) const {
  return url == other.url && request_start == other.request_start &&
         first_party_for_cookies == other.first_party_for_cookies &&
         tab_id == other.tab_id && connection_type == other.connection_type;
}

}  // namespace data_usage
