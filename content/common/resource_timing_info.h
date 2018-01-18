// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_TIMING_INFO_H_
#define CONTENT_COMMON_RESOURCE_TIMING_INFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/optional.h"

namespace content {

// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/WebKit.
// See blink::WebServerTimingInfo for more information about this struct's
// fields.
struct ServerTimingInfo {
  ServerTimingInfo();
  ServerTimingInfo(const ServerTimingInfo&);
  ~ServerTimingInfo();

  std::string name;
  double duration = 0.0;
  std::string description;
};

// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/WebKit.
// See blink::WebURLLoadTiming for more information about this struct's fields.
struct ResourceLoadTiming {
  ResourceLoadTiming();
  ResourceLoadTiming(const ResourceLoadTiming&);
  ~ResourceLoadTiming();

  double request_time = 0.0;
  double proxy_start = 0.0;
  double proxy_end = 0.0;
  double dns_start = 0.0;
  double dns_end = 0.0;
  double connect_start = 0.0;
  double connect_end = 0.0;
  double worker_start = 0.0;
  double worker_ready = 0.0;
  double send_start = 0.0;
  double send_end = 0.0;
  double receive_headers_end = 0.0;
  double ssl_start = 0.0;
  double ssl_end = 0.0;
  double push_start = 0.0;
  double push_end = 0.0;
};

// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/WebKit.
// See blink::WebResourceTimingInfo for more information about this struct's
// fields.
struct ResourceTimingInfo {
  ResourceTimingInfo();
  ResourceTimingInfo(const ResourceTimingInfo&);
  ~ResourceTimingInfo();

  std::string name;
  double start_time = 0.0;
  std::string initiator_type;
  std::string alpn_negotiated_protocol;
  std::string connection_info;
  base::Optional<ResourceLoadTiming> timing;
  double last_redirect_end_time = 0.0;
  double finish_time = 0.0;
  uint64_t transfer_size = 0;
  uint64_t encoded_body_size = 0;
  uint64_t decoded_body_size = 0;
  bool did_reuse_connection = false;
  bool allow_timing_details = false;
  bool allow_redirect_details = false;
  bool allow_negative_values = false;
  std::vector<ServerTimingInfo> server_timing;
};

}  // namespace content

#endif  // CONTENT_COMMON_RESOURCE_TIMING_INFO_H_
