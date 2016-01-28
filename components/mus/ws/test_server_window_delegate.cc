// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate() : root_window_(nullptr) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

mus::SurfacesState* TestServerWindowDelegate::GetSurfacesState() {
  return nullptr;
}

void TestServerWindowDelegate::OnScheduleWindowPaint(
    const ServerWindow* window) {}

const ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) const {
  return root_window_;
}

void TestServerWindowDelegate::ScheduleSurfaceDestruction(
    ServerWindow* window) {}

ServerWindow* TestServerWindowDelegate::FindWindowForSurface(
    const ServerWindow* ancestor,
    mojom::SurfaceType surface_type,
    const ClientWindowId& client_window_id) {
  return nullptr;
}

}  // namespace ws

}  // namespace mus
