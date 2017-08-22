// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_NET_EVENT_LOGGER_H_
#define COMPONENTS_SAFE_BROWSING_NET_EVENT_LOGGER_H_

#include "base/macros.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

class GURL;

namespace safe_browsing {

// A helper class to log network events for SafeBrowsing.
class NetEventLogger {
 public:
  // |net_log| must outlive this class.
  explicit NetEventLogger(const net::NetLogWithSource* net_log);

  // For marking network events.  |name| and |value| can be null.
  void BeginNetLogEvent(net::NetLogEventType type,
                        const GURL& url,
                        const char* name,
                        const char* value);
  void EndNetLogEvent(net::NetLogEventType type,
                      const char* name,
                      const char* value);

 private:
  const net::NetLogWithSource* net_log_;
  net::NetLogWithSource net_log_with_sb_source_;

  DISALLOW_COPY_AND_ASSIGN(NetEventLogger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_NET_EVENT_LOGGER_H_
