// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_viewer/chrome_metro_viewer_process_host_aurawin.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_remote_window_tree_host_win.h"
#include "ash/shell.h"
#include "ash/wm/window_positioner.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_aurawin.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/env_vars.h"
#include "components/search_engines/util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/gfx/win/dpi.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "url/gurl.h"

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

void OpenURL(const GURL& url) {
  chrome::NavigateParams params(
      ProfileManager::GetActiveUserProfile(),
      GURL(url),
      ui::PAGE_TRANSITION_TYPED);
  params.disposition = NEW_FOREGROUND_TAB;
  params.host_desktop_type = chrome::HOST_DESKTOP_TYPE_ASH;
  chrome::Navigate(&params);
}

}  // namespace

ChromeMetroViewerProcessHost::ChromeMetroViewerProcessHost()
    : MetroViewerProcessHost(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO)) {
  chrome::IncrementKeepAliveCount();
}

ChromeMetroViewerProcessHost::~ChromeMetroViewerProcessHost() {
}

void ChromeMetroViewerProcessHost::OnChannelError() {
  // TODO(cpu): At some point we only close the browser. Right now this
  // is very convenient for developing.
  DVLOG(1) << "viewer channel error : Quitting browser";

  // Unset environment variable to let breakpad know that metro process wasn't
  // connected.
  ::SetEnvironmentVariableA(env_vars::kMetroConnected, NULL);

  aura::RemoteWindowTreeHostWin::Instance()->Disconnected();
  chrome::DecrementKeepAliveCount();

  // If browser is trying to quit, we shouldn't reenter the process.
  // TODO(shrikant): In general there seem to be issues with how AttemptExit
  // reentry works. In future release please clean up related code.
  if (!browser_shutdown::IsTryingToQuit()) {
    CloseOpenAshBrowsers();
    chrome::CloseAsh();
  }
  // Tell the rest of Chrome about it.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_ASH_SESSION_ENDED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // This will delete the MetroViewerProcessHost object. Don't access member
  // variables/functions after this call.
  g_browser_process->platform_part()->OnMetroViewerProcessTerminated();
}

void ChromeMetroViewerProcessHost::OnChannelConnected(int32 /*peer_pid*/) {
  DVLOG(1) << "ChromeMetroViewerProcessHost::OnChannelConnected: ";
  // Set environment variable to let breakpad know that metro process was
  // connected.
  ::SetEnvironmentVariableA(env_vars::kMetroConnected, "1");

  if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(NULL)) {
    DVLOG(1) << "No GPU access, attempting to restart in Desktop\n";
    chrome::AttemptRestartToDesktopMode();
  }
}

void ChromeMetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface,
    float device_scale) {
  HWND hwnd = reinterpret_cast<HWND>(target_surface);

  gfx::InitDeviceScaleFactor(device_scale);
  chrome::OpenAsh(hwnd);
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  DCHECK_EQ(hwnd, aura::RemoteWindowTreeHostWin::Instance()->remote_window());
  ash::Shell::GetInstance()->CreateShelf();
  ash::Shell::GetInstance()->ShowShelf();

  // Tell our root window host that the viewer has connected.
  aura::RemoteWindowTreeHostWin::Instance()->Connected(this);

  // On Windows 8 ASH we default to SHOW_STATE_MAXIMIZED for the browser
  // window. This is to ensure that we honor metro app conventions by default.
  ash::WindowPositioner::SetMaximizeFirstWindow(true);
  // Tell the rest of Chrome that Ash is running.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_ASH_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void ChromeMetroViewerProcessHost::OnOpenURL(const base::string16& url) {
  OpenURL(GURL(url));
}

void ChromeMetroViewerProcessHost::OnHandleSearchRequest(
    const base::string16& search_string) {
  GURL url(GetDefaultSearchURLForSearchTerms(
      TemplateURLServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile()), search_string));
  if (url.is_valid())
    OpenURL(url);
}

void ChromeMetroViewerProcessHost::OnWindowSizeChanged(uint32 width,
                                                       uint32 height) {
  std::vector<ash::DisplayInfo> info_list;
  info_list.push_back(ash::DisplayInfo::CreateFromSpec(
      base::StringPrintf("%dx%d*%f", width, height, gfx::GetDPIScale())));
  ash::Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(
      info_list);
  aura::RemoteWindowTreeHostWin::Instance()->HandleWindowSizeChanged(width,
                                                                     height);
}
