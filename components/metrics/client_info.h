// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLIENT_INFO_H_
#define COMPONENTS_METRICS_CLIENT_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"

namespace metrics {

// A data object used to pass data from outside the metrics component into the
// metrics component.
struct ClientInfo {
 public:
  ClientInfo();
  ~ClientInfo();

  // The metrics ID of this client: represented as a GUID string.
  std::string client_id;

  // The installation date: represented as an epoch time in seconds.
  int64 installation_date;

  // The date on which metrics reporting was enabled: represented as an epoch
  // time in seconds.
  int64 reporting_enabled_date;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientInfo);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLIENT_INFO_H_
