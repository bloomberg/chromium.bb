// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

namespace content {
namespace devtools {
namespace network {

typedef DevToolsProtocolClient::Response Response;

NetworkHandler::NetworkHandler() {
}

NetworkHandler::~NetworkHandler() {
}

Response NetworkHandler::ClearBrowserCache() {
  return Response::FallThrough();
}

Response NetworkHandler::ClearBrowserCookies() {
  return Response::FallThrough();
}

Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  return Response::FallThrough();
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
