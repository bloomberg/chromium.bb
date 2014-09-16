// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_
#define COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_

#include <string>

#include "base/json/json_value_converter.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"

namespace net {
class URLRequestContextBuilder;
}  // namespace net

namespace cronet {

// Common configuration parameters used by Cronet to configure
// URLRequestContext. Can be parsed from JSON string passed through JNI.
struct URLRequestContextConfig {
  // App-provided hint that server supports QUIC.
  struct QuicHint {
    QuicHint();
    ~QuicHint();

    // Register |converter| for use in converter.Convert().
    static void RegisterJSONConverter(
        base::JSONValueConverter<QuicHint>* converter);

    // Host name of the server that supports QUIC.
    std::string host;
    // Port of the server that supports QUIC.
    int port;
    // Alternate protocol port.
    int alternate_port;

   private:
    DISALLOW_COPY_AND_ASSIGN(QuicHint);
  };

  URLRequestContextConfig();
  ~URLRequestContextConfig();

  // Configure |context_builder| based on |this|.
  void ConfigureURLRequestContextBuilder(
      net::URLRequestContextBuilder* context_builder);

  // Register |converter| for use in converter.Convert().
  static void RegisterJSONConverter(
      base::JSONValueConverter<URLRequestContextConfig>* converter);

  // Enable QUIC.
  bool enable_quic;
  // Enable SPDY.
  bool enable_spdy;
  // Type of http cache: "HTTP_CACHE_DISABLED", "HTTP_CACHE_DISK" or
  // "HTTP_CACHE_IN_MEMORY".
  std::string http_cache;
  // Max size of http cache in bytes.
  int http_cache_max_size;
  // Storage path for http cache and cookie storage.
  std::string storage_path;
  // App-provided list of servers that support QUIC.
  ScopedVector<QuicHint> quic_hints;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestContextConfig);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_
