// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
namespace devtools {
namespace network {

typedef DevToolsProtocolClient::Response Response;

NetworkHandler::NetworkHandler() : host_(nullptr) {
}

NetworkHandler::~NetworkHandler() {
}

void NetworkHandler::SetRenderViewHost(RenderViewHost* host) {
  host_ = host;
}

Response NetworkHandler::ClearBrowserCache() {
  if (host_)
    GetContentClient()->browser()->ClearCache(host_);
  return Response::OK();
}

Response NetworkHandler::ClearBrowserCookies() {
  if (host_)
    GetContentClient()->browser()->ClearCookies(host_);
  return Response::OK();
}

Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  *result = false;
  return Response::OK();
}

Response NetworkHandler::EmulateNetworkConditions(bool offline,
                                                  double latency,
                                                  double download_throughput,
                                                  double upload_throughput) {
  return Response::FallThrough();
}

}  // namespace dom
}  // namespace devtools
}  // namespace content
