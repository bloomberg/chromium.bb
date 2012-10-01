// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "ui/surface/accelerated_surface_win.h"

MetroViewerProcessHost::MetroViewerProcessHost() {
  channel_.reset(new IPC::ChannelProxy(
      // TODO(scottmg): Need to have a secure way to randomize and request
      // this name from the viewer-side.
      "viewer",
      IPC::Channel::MODE_NAMED_SERVER,
      this,
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO)));
}

MetroViewerProcessHost::~MetroViewerProcessHost() {
}

bool MetroViewerProcessHost::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool MetroViewerProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MetroViewerProcessHost, message)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetTargetSurface, OnSetTargetSurface)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseEvent, OnMouseEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface) {
  DLOG(INFO) << __FUNCTION__ << ", target_surface = " << target_surface;
  HWND hwnd = reinterpret_cast<HWND>(target_surface);

  scoped_refptr<AcceleratedPresenter> any_window =
      AcceleratedPresenter::GetForWindow(NULL);
  any_window->SetNewTargetWindow(hwnd);
}

void MetroViewerProcessHost::OnMouseEvent(
    int msg, WPARAM w_param, LPARAM l_param) {
  // TODO(scottmg): Pass to window.
}
