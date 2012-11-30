// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "ui/surface/accelerated_surface_win.h"

namespace {

void CloseOpenAshBrowsers() {
  chrome::BrowserListImpl* browser_list =
      chrome::BrowserListImpl::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  if (browser_list) {
    for (chrome::BrowserListImpl::const_iterator i = browser_list->begin();
            i != browser_list->end(); ++i) {
      Browser* browser = *i;
      browser->window()->Close();
      // If the attempt to Close the browser fails due to unload handlers on
      // the page or in progress downloads, etc, destroy all tabs on the page.
      while (browser->tab_count())
        delete browser->tab_strip_model()->GetTabContentsAt(0);
    }
  }
}

}  // namespace


MetroViewerProcessHost::MetroViewerProcessHost(
    const std::string& ipc_channel_name) {
  g_browser_process->AddRefModule();
  channel_.reset(new IPC::ChannelProxy(
      ipc_channel_name.c_str(),
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
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseMoved, OnMouseMoved)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseButton, OnMouseButton)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyDown, OnKeyDown)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyUp, OnKeyUp)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_Character, OnChar)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_VisibilityChanged,
                        OnVisibilityChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void  MetroViewerProcessHost::OnChannelError() {
  // TODO(cpu): At some point we only close the browser. Right now this
  // is very convenient for developing.
  DLOG(INFO) << "viewer channel error : Quitting browser";
  g_browser_process->ReleaseModule();
  CloseOpenAshBrowsers();
  chrome::CloseAsh();
}

void MetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface) {
  DLOG(INFO) << __FUNCTION__ << ", target_surface = " << target_surface;
  HWND hwnd = reinterpret_cast<HWND>(target_surface);

  chrome::OpenAsh();

  scoped_refptr<AcceleratedPresenter> any_window =
      AcceleratedPresenter::GetForWindow(NULL);
  any_window->SetNewTargetWindow(hwnd);
}

// TODO(cpu): Find a decent way to get to the root window host in the
// next four methods.
void MetroViewerProcessHost::OnMouseMoved(int32 x, int32 y, int32 flags) {
  aura::RemoteRootWindowHostWin::Instance()->OnMouseMoved(x, y, flags);
}

void MetroViewerProcessHost::OnMouseButton(
    int32 x, int32 y, int32 extra, ui::EventType type, ui::EventFlags flags) {
  aura::RemoteRootWindowHostWin::Instance()->OnMouseButton(
      x, y, extra, type, flags);
}

void MetroViewerProcessHost::OnKeyDown(uint32 vkey,
                                       uint32 repeat_count,
                                       uint32 scan_code,
                                       uint32 flags) {
  aura::RemoteRootWindowHostWin::Instance()->OnKeyDown(
      vkey, repeat_count, scan_code, flags);
}

void MetroViewerProcessHost::OnKeyUp(uint32 vkey,
                                     uint32 repeat_count,
                                     uint32 scan_code,
                                     uint32 flags) {
  aura::RemoteRootWindowHostWin::Instance()->OnKeyUp(
      vkey, repeat_count, scan_code, flags);
}

void MetroViewerProcessHost::OnChar(uint32 key_code,
                                    uint32 repeat_count,
                                    uint32 scan_code,
                                    uint32 flags) {
  aura::RemoteRootWindowHostWin::Instance()->OnChar(
      key_code, repeat_count, scan_code, flags);
}

void MetroViewerProcessHost::OnVisibilityChanged(bool visible) {
  aura::RemoteRootWindowHostWin::Instance()->OnVisibilityChanged(
      visible);
}
