// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_metro_viewer_process_host.h"

#include <shellapi.h>
#include <shlobj.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "ui/surface/accelerated_surface_win.h"

namespace ash {
namespace test {

TestMetroViewerProcessHost::TestMetroViewerProcessHost(
    const std::string& ipc_channel_name,
    base::SingleThreadTaskRunner* ipc_task_runner)
        : MetroViewerProcessHost(ipc_channel_name, ipc_task_runner),
          closed_unexpectedly_(false) {
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

  backing_surface.reset(new AcceleratedSurface(hwnd));

  scoped_refptr<AcceleratedPresenter> any_window =
      AcceleratedPresenter::GetForWindow(NULL);
  any_window->SetNewTargetWindow(hwnd);
  aura::RemoteRootWindowHostWin::Instance()->Connected(this);
}

void TestMetroViewerProcessHost::OnOpenURL(const string16& url) {
}

void TestMetroViewerProcessHost::OnHandleSearchRequest(
    const string16& search_string) {
}

}  // namespace test
}  // namespace ash
