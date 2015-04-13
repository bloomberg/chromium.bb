// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

namespace content {

class RenderFrameHost;

namespace devtools {
namespace network {

class NetworkHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  NetworkHandler();
  virtual ~NetworkHandler();

  void SetRenderFrameHost(RenderFrameHost* host);
  void SetClient(scoped_ptr<DevToolsProtocolClient> client);

  Response ClearBrowserCache();
  Response ClearBrowserCookies();
  Response GetCookies(DevToolsCommandId command_id);
  Response DeleteCookie(DevToolsCommandId command_id,
                        const std::string& cookie_name,
                        const std::string& url);

  Response CanEmulateNetworkConditions(bool* result);
  Response EmulateNetworkConditions(bool offline,
                                    double latency,
                                    double download_throughput,
                                    double upload_throughput);

 private:
  RenderFrameHost* host_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace network
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
