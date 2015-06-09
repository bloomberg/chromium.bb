// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

namespace content {
namespace devtools {
namespace security {

typedef DevToolsProtocolClient::Response Response;

SecurityHandler::SecurityHandler() {
}

SecurityHandler::~SecurityHandler() {
}

void SecurityHandler::SetClient(scoped_ptr<DevToolsProtocolClient> client) {
}

Response SecurityHandler::Enable() {
  return Response::OK();
}

Response SecurityHandler::Disable() {
  return Response::OK();
}

}  // namespace security
}  // namespace devtools
}  // namespace content
