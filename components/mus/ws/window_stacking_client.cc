// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_stacking_client.h"

namespace mus {
namespace ws {

namespace {

WindowStackingClient* instance = nullptr;

}  // namespace

void SetWindowStackingClient(WindowStackingClient* client) {
  instance = client;
}

WindowStackingClient* GetWindowStackingClient() {
  return instance;
}

}  // namespace ws
}  // namespace mus
