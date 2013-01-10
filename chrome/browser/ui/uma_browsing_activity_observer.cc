// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/uma_browsing_activity_observer.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

namespace chrome {
namespace {

UMABrowsingActivityObserver* g_instance = NULL;

}  // namespace

// static
void UMABrowsingActivityObserver::Init() {
  DCHECK(!g_instance);
  DCHECK(BrowserList::empty());  // Must be created before any Browsers are.
  g_instance = new UMABrowsingActivityObserver;
}

UMABrowsingActivityObserver::UMABrowsingActivityObserver() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                  content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                  content::NotificationService::AllSources());
}

UMABrowsingActivityObserver::~UMABrowsingActivityObserver() {
}

void UMABrowsingActivityObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    const content::LoadCommittedDetails& load =
        *content::Details<content::LoadCommittedDetails>(details).ptr();
    if (!load.is_navigation_to_different_page())
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  } else if (type == chrome::NOTIFICATION_APP_TERMINATING) {
    delete g_instance;
    g_instance = NULL;
  }
}

void UMABrowsingActivityObserver::LogRenderProcessHostCount() const {
  int hosts_count = 0;
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
        !i.IsAtEnd(); i.Advance())
    ++hosts_count;
  UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count,
                              1, 50, 50);
}

void UMABrowsingActivityObserver::LogBrowserTabCount() const {
  int tab_count = 0;
  int app_window_count = 0;
  int panel_window_count = 0;
  int popup_window_count = 0;
  int tabbed_window_count = 0;
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    // Record how many tabs each window has open.
    Browser* browser = (*browser_iterator);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                browser->tab_strip_model()->count(),
                                1, 200, 50);
    tab_count += browser->tab_strip_model()->count();

    if (browser->window()->IsActive()) {
      // Record how many tabs the active window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountActiveWindow",
                                  browser->tab_strip_model()->count(),
                                  1, 200, 50);
    }

    if (browser->is_app())
      app_window_count++;
    else if (browser->is_type_panel())
      panel_window_count++;
    else if (browser->is_type_popup())
      popup_window_count++;
    else if (browser->is_type_tabbed())
      tabbed_window_count++;
  }
  // Record how many tabs total are open (across all windows).
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);

  // Record how many windows are open, by type.
  UMA_HISTOGRAM_COUNTS_100("WindowManager.AppWindowCountPerLoad",
                           app_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.PanelWindowCountPerLoad",
                           panel_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.PopUpWindowCountPerLoad",
                           popup_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.TabbedWindowCountPerLoad",
                           tabbed_window_count);
}

}  // namespace chrome
