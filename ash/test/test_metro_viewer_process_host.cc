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

TestMetroViewerProcessHost::InternalMessageFilter::InternalMessageFilter(
    TestMetroViewerProcessHost* owner)
        : owner_(owner) {
}

void TestMetroViewerProcessHost::InternalMessageFilter::OnChannelConnected(
    int32 peer_pid) {
  owner_->NotifyChannelConnected();
}

TestMetroViewerProcessHost::TestMetroViewerProcessHost(
    const std::string& ipc_channel_name)
        : ipc_thread_("test_metro_viewer_ipc_thread"),
          channel_connected_event_(false, false),
          closed_unexpectedly_(false) {

  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ipc_thread_.StartWithOptions(options);

  channel_.reset(new IPC::ChannelProxy(
      ipc_channel_name.c_str(),
      IPC::Channel::MODE_NAMED_SERVER,
      this,
      ipc_thread_.message_loop_proxy()));

  channel_->AddFilter(new InternalMessageFilter(this));
}

TestMetroViewerProcessHost::~TestMetroViewerProcessHost() {
  channel_.reset();
  ipc_thread_.Stop();
}

void TestMetroViewerProcessHost::NotifyChannelConnected() {
  channel_connected_event_.Signal();
}

bool TestMetroViewerProcessHost::LaunchViewerAndWaitForConnection(
    const string16& app_user_model_id) {
  // Activate the viewer process. NOTE: This assumes that the viewer process is
  // registered as the default browser using the provided |app_user_model_id|.

  // TODO(robertshield): Initialize COM at test suite startup.
  base::win::ScopedCOMInitializer com_initializer;

  base::win::ScopedComPtr<IApplicationActivationManager> activator;
  HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    hr = activator->ActivateApplication(
        app_user_model_id.c_str(), L"open", AO_NONE, &pid);
  }

  LOG_IF(ERROR, FAILED(hr)) << "Tried and failed to launch Metro Chrome. "
                            << "hr=" << std::hex << hr;

  // Having launched the viewer process, now we wait for it to connect.
  return channel_connected_event_.TimedWait(base::TimeDelta::FromSeconds(60));
}

bool TestMetroViewerProcessHost::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool TestMetroViewerProcessHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TestMetroViewerProcessHost, message)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetTargetSurface, OnSetTargetSurface)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled ? true :
      aura::RemoteRootWindowHostWin::Instance()->OnMessageReceived(message);
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

}  // namespace test
}  // namespace ash
