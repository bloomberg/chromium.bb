// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/inspector_handler.h"

namespace content {
namespace devtools {
namespace inspector {

InspectorHandler::InspectorHandler() {
}

InspectorHandler::~InspectorHandler() {
}

void InspectorHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void InspectorHandler::TargetCrashed() {
  client_->TargetCrashed(TargetCrashedParams::Create());
}

}  // namespace inspector
}  // namespace devtools
}  // namespace content
