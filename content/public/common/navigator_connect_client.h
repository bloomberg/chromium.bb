// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_NAVIGATOR_CONNECT_CLIENT_H_
#define CONTENT_PUBLIC_COMMON_NAVIGATOR_CONNECT_CLIENT_H_

#include "url/gurl.h"

namespace content {

// This struct represents a connection to a navigator.connect exposed service.
struct NavigatorConnectClient {
  NavigatorConnectClient();
  NavigatorConnectClient(const GURL& target_url,
                         const GURL& origin,
                         int message_port_id);
  ~NavigatorConnectClient();

  // The URL this client is connected to (or trying to connect to).
  GURL target_url;

  // The origin of the client.
  GURL origin;

  // Message port ID for the service side message port associated with this
  // client.
  int message_port_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_NAVIGATOR_CONNECT_CLIENT_H_
