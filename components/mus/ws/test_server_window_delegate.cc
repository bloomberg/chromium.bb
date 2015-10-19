// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

TestServerWindowDelegate::TestServerWindowDelegate() : root_window_(nullptr) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

SurfacesState* TestServerWindowDelegate::GetSurfacesState() {
  return nullptr;
}

void TestServerWindowDelegate::OnScheduleWindowPaint(
    const ServerWindow* window) {}

const ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) const {
  return root_window_;
}

}  // namespace mus
