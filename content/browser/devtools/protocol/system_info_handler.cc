// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

namespace content {
namespace devtools {
namespace system_info {

typedef DevToolsProtocolClient::Response Response;

SystemInfoHandler::SystemInfoHandler() {
}

SystemInfoHandler::~SystemInfoHandler() {
}

Response SystemInfoHandler::GetInfo(scoped_refptr<SystemInfo>* info) {
  return Response::FallThrough();
}

}  // namespace system_info
}  // namespace devtools
}  // namespace content
