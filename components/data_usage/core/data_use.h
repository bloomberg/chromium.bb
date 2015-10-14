// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USAGE_CORE_DATA_USE_H_
#define COMPONENTS_DATA_USAGE_CORE_DATA_USE_H_

#include <stdint.h>

#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

namespace data_usage {

struct DataUse {
  DataUse(const GURL& url,
          const base::Time& request_time,
          const GURL& first_party_for_cookies,
          int32_t tab_id,
          net::NetworkChangeNotifier::ConnectionType connection_type,
          int64_t tx_bytes,
          int64_t rx_bytes);

  // Returns true if |this| and |other| are identical except for byte counts.
  bool CanCombineWith(const DataUse& other) const;

  GURL url;
  // The time when the request that is associated with these bytes was started.
  base::Time request_time;
  GURL first_party_for_cookies;
  int32_t tab_id;
  net::NetworkChangeNotifier::ConnectionType connection_type;
  // TODO(sclittle): Add more network info here, e.g. for Android, SIM operator
  // and whether the device is roaming.

  int64_t tx_bytes;
  int64_t rx_bytes;
};

}  // namespace data_usage

#endif  // COMPONENTS_DATA_USAGE_CORE_DATA_USE_H_
