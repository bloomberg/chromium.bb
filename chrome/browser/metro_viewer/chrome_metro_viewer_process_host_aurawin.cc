// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_viewer/chrome_metro_viewer_process_host_aurawin.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_aurawin.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/env_vars.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/surface/accelerated_surface_win.h"
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
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  browser->OpenURL(content::OpenURLParams(
    GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
    content::PAGE_TRANSITION_TYPED, false));
}

}  // namespace

ChromeMetroViewerProcessHost::ChromeMetroViewerProcessHost()
    : MetroViewerProcessHost(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO)) {
  g_browser_process->AddRefModule();
}

void ChromeMetroViewerProcessHost::OnChannelError() {
  // TODO(cpu): At some point we only close the browser. Right now this
  // is very convenient for developing.
  DLOG(INFO) << "viewer channel error : Quitting browser";

  // Unset environment variable to let breakpad know that metro process wasn't
  // connected.
  ::SetEnvironmentVariableA(env_vars::kMetroConnected, NULL);

  aura::RemoteRootWindowHostWin::Instance()->Disconnected();
  g_browser_process->ReleaseModule();
  CloseOpenAshBrowsers();
  chrome::CloseAsh();
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
  DLOG(INFO) << "ChromeMetroViewerProcessHost::OnChannelConnected: ";
  // Set environment variable to let breakpad know that metro process was
  // connected.
  ::SetEnvironmentVariableA(env_vars::kMetroConnected, "1");
}

void ChromeMetroViewerProcessHost::OnSetTargetSurface(
    gfx::NativeViewId target_surface) {
  HWND hwnd = reinterpret_cast<HWND>(target_surface);
  // Tell our root window host that the viewer has connected.
  aura::RemoteRootWindowHostWin::Instance()->Connected(this, hwnd);
  // Now start the Ash shell environment.
  chrome::OpenAsh();
  ash::Shell::GetInstance()->CreateLauncher();
  ash::Shell::GetInstance()->ShowLauncher();
  // Tell the rest of Chrome that Ash is running.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_ASH_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void ChromeMetroViewerProcessHost::OnOpenURL(const string16& url) {
  OpenURL(GURL(url));
}

void ChromeMetroViewerProcessHost::OnHandleSearchRequest(
    const string16& search_string) {
  GURL url(GetDefaultSearchURLForSearchTerms(
      ProfileManager::GetDefaultProfileOrOffTheRecord(), search_string));
  if (url.is_valid())
    OpenURL(url);
}
