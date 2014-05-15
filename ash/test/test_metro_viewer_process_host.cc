// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_metro_viewer_process_host.h"

#include <windef.h>

#include "base/logging.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/gfx/win/dpi.h"

namespace ash {
namespace test {

TestMetroViewerProcessHost::TestMetroViewerProcessHost(
    base::SingleThreadTaskRunner* ipc_task_runner)
    : MetroViewerProcessHost(ipc_task_runner),
      closed_unexpectedly_(false) {
}

TestMetroViewerProcessHost::~TestMetroViewerProcessHost() {
}

void TestMetroViewerProcessHost::OnChannelError() {
  closed_unexpectedly_ = true;
  aura::RemoteWindowTreeHostWin::Instance()->Disconnected();
}

void TestMetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface,
    float device_scale) {
  DLOG(INFO) << __FUNCTION__ << ", target_surface = " << target_surface;
  HWND hwnd = reinterpret_cast<HWND>(target_surface);
  gfx::InitDeviceScaleFactor(device_scale);
  aura::RemoteWindowTreeHostWin::Instance()->SetRemoteWindowHandle(hwnd);
  aura::RemoteWindowTreeHostWin::Instance()->Connected(this);
}

void TestMetroViewerProcessHost::OnOpenURL(const base::string16& url) {
}

void TestMetroViewerProcessHost::OnHandleSearchRequest(
    const base::string16& search_string) {
}

void TestMetroViewerProcessHost::OnWindowSizeChanged(uint32 width,
                                                     uint32 height) {
}

}  // namespace test
}  // namespace ash
