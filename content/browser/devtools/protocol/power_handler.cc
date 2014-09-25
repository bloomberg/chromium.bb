// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/power_handler.h"

namespace content {
namespace devtools {
namespace power {

typedef DevToolsProtocolClient::Response Response;

PowerHandler::PowerHandler() {
}

PowerHandler::~PowerHandler() {
}

void PowerHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

Response PowerHandler::Start() {
  return Response::FallThrough();
}

Response PowerHandler::End() {
  return Response::FallThrough();
}

Response PowerHandler::CanProfilePower(bool* result) {
  return Response::FallThrough();
}

Response PowerHandler::GetAccuracyLevel(std::string* result) {
  return Response::FallThrough();
}

}  // namespace power
}  // namespace devtools
}  // namespace content
