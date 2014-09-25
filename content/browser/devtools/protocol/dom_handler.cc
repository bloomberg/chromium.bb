// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/dom_handler.h"

namespace content {
namespace devtools {
namespace dom {

typedef DevToolsProtocolClient::Response Response;

DOMHandler::DOMHandler() {
}

DOMHandler::~DOMHandler() {
}

Response DOMHandler::SetFileInputFiles(NodeId node_id,
                                       const std::vector<std::string>& files) {
  return Response::FallThrough();
}

}  // namespace dom
}  // namespace devtools
}  // namespace content
