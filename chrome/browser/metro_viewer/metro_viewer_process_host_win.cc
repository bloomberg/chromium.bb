// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "ui/surface/accelerated_surface_win.h"

namespace {

void CloseOpenAshBrowsers() {
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  if (browser_list) {
    for (BrowserList::const_iterator i = browser_list->begin();
         i != browser_list->end(); ++i) {
      Browser* browser = *i;
      browser->window()->Close();
      // If the attempt to Close the browser fails due to unload handlers on
      // the page or in progress downloads, etc, destroy all tabs on the page.
      while (browser->tab_strip_model()->count())
        delete browser->tab_strip_model()->GetWebContentsAt(0);
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled ? true :
      aura::RemoteRootWindowHostWin::Instance()->OnMessageReceived(message);
}

void  MetroViewerProcessHost::OnChannelError() {
  // TODO(cpu): At some point we only close the browser. Right now this
  // is very convenient for developing.
  DLOG(INFO) << "viewer channel error : Quitting browser";
  aura::RemoteRootWindowHostWin::Instance()->Disconnected();
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
  aura::RemoteRootWindowHostWin::Instance()->Connected(this);
}
