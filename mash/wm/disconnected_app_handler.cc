// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/disconnected_app_handler.h"

namespace mash {
namespace wm {

DisconnectedAppHandler::DisconnectedAppHandler() {}

DisconnectedAppHandler::~DisconnectedAppHandler() {}

void DisconnectedAppHandler::OnWindowEmbeddedAppDisconnected(
    mus::Window* window) {
  window->Destroy();
}

}  // namespace wm
}  // namespace mash
