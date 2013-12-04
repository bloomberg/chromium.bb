// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_metro_viewer_process_host.h"

#include <windef.h>

#include "base/logging.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/surface/accelerated_surface_win.h"

namespace ash {
namespace test {

TestMetroViewerProcessHost::TestMetroViewerProcessHost(
    base::SingleThreadTaskRunner* ipc_task_runner)
        : MetroViewerProcessHost(ipc_task_runner), closed_unexpectedly_(false) {
}

TestMetroViewerProcessHost::~TestMetroViewerProcessHost() {
}

void TestMetroViewerProcessHost::OnChannelError() {
  closed_unexpectedly_ = true;
  aura::RemoteRootWindowHostWin::Instance()->Disconnected();
}

void TestMetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface) {
  DLOG(INFO) << __FUNCTION__ << ", target_surface = " << target_surface;
  HWND hwnd = reinterpret_cast<HWND>(target_surface);
  aura::RemoteRootWindowHostWin::Instance()->Connected(this, hwnd);

  backing_surface_.reset(new AcceleratedSurface(hwnd));
}

void TestMetroViewerProcessHost::OnOpenURL(const string16& url) {
}

void TestMetroViewerProcessHost::OnHandleSearchRequest(
    const string16& search_string) {
}

void TestMetroViewerProcessHost::OnWindowSizeChanged(uint32 width,
                                                     uint32 height) {
}

}  // namespace test
}  // namespace ash
