// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/uma_browsing_activity_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/search_engines/template_url_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

namespace chrome {
namespace {

UMABrowsingActivityObserver* g_uma_browsing_activity_observer_instance = NULL;

}  // namespace

// static
void UMABrowsingActivityObserver::Init() {
  DCHECK(!g_uma_browsing_activity_observer_instance);
  // Must be created before any Browsers are.
  DCHECK_EQ(0U, chrome::GetTotalBrowserCount());
  g_uma_browsing_activity_observer_instance = new UMABrowsingActivityObserver;
}

UMABrowsingActivityObserver::UMABrowsingActivityObserver() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

UMABrowsingActivityObserver::~UMABrowsingActivityObserver() {}

void UMABrowsingActivityObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    const content::LoadCommittedDetails load =
        *content::Details<content::LoadCommittedDetails>(details).ptr();

    content::NavigationController* controller =
        content::Source<content::NavigationController>(source).ptr();
    // Track whether the page loaded is a search results page (SRP). Track
    // the non-SRP navigations as well so there is a control.
    base::RecordAction(base::UserMetricsAction("NavEntryCommitted"));
    // Attempting to determine the cause of a crash originating from
    // IsSearchResultsPageFromDefaultSearchProvider but manifesting in
    // TemplateURLRef::ExtractSearchTermsFromURL(...).
    // See http://crbug.com/291348.
    CHECK(load.entry);
    if (TemplateURLServiceFactory::GetForProfile(
            Profile::FromBrowserContext(controller->GetBrowserContext()))
            ->IsSearchResultsPageFromDefaultSearchProvider(
                load.entry->GetURL())) {
      base::RecordAction(base::UserMetricsAction("NavEntryCommitted.SRP"));
    }

    if (!load.is_navigation_to_different_page())
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  } else if (type == chrome::NOTIFICATION_APP_TERMINATING) {
    LogTimeBeforeUpdate();
    delete g_uma_browsing_activity_observer_instance;
    g_uma_browsing_activity_observer_instance = NULL;
  }
}

void UMABrowsingActivityObserver::LogTimeBeforeUpdate() const {
  const base::Time upgrade_detected_time =
      UpgradeDetector::GetInstance()->upgrade_detected_time();
  if (upgrade_detected_time.is_null())
    return;
  const base::Time now = base::Time::Now();
  UMA_HISTOGRAM_EXACT_LINEAR(
      "UpgradeDetector.DaysBeforeUpgrade",
      base::TimeDelta(now - upgrade_detected_time).InDays(), 30);
}

void UMABrowsingActivityObserver::LogRenderProcessHostCount() const {
  int hosts_count = 0;
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++hosts_count;
  UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count, 1, 50, 50);
}

void UMABrowsingActivityObserver::LogBrowserTabCount() const {
  int tab_count = 0;
  int tab_group_count = 0;
  int customized_tab_group_count = 0;
  int app_window_count = 0;
  int popup_window_count = 0;
  int tabbed_window_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    // Record how many tabs each window has open.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                browser->tab_strip_model()->count(), 1, 200,
                                50);
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    tab_count += tab_strip_model->count();

    const std::vector<tab_groups::TabGroupId>& groups =
        tab_strip_model->group_model()->ListTabGroups();
    tab_group_count += groups.size();
    for (const tab_groups::TabGroupId& group_id : groups) {
      const TabGroup* const tab_group =
          tab_strip_model->group_model()->GetTabGroup(group_id);
      if (tab_group->IsCustomized() ||
          !tab_group->visual_data()->title().empty()) {
        ++customized_tab_group_count;
      }
    }

    if (browser->window()->IsActive()) {
      // Record how many tabs the active window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountActiveWindow",
                                  browser->tab_strip_model()->count(), 1, 200,
                                  50);
    }

    if (browser->deprecated_is_app())
      app_window_count++;
    else if (browser->is_type_popup())
      popup_window_count++;
    else if (browser->is_type_normal())
      tabbed_window_count++;
  }
  // Record how many tabs total are open (across all windows).
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);

  // Record how many tab groups are open across all windows.
  UMA_HISTOGRAM_COUNTS_100("TabGroups.UserGroupCountPerLoad", tab_group_count);

  // Record how many tabs are in the current group. Records 0 if the active tab
  // is not in a group.
  const Browser* current_browser = BrowserList::GetInstance()->GetLastActive();
  if (current_browser) {
    TabStripModel* const tab_strip_model = current_browser->tab_strip_model();
    const base::Optional<tab_groups::TabGroupId> active_group =
        tab_strip_model->GetTabGroupForTab(tab_strip_model->active_index());
    UMA_HISTOGRAM_COUNTS_100("Tabs.TabCountInGroupPerLoad",
                             active_group.has_value()
                                 ? tab_strip_model->group_model()
                                       ->GetTabGroup(active_group.value())
                                       ->ListTabs()
                                       .size()
                                 : 0);
  }

  // Record how many tab groups with a user-set name or color are open across
  // all windows.
  UMA_HISTOGRAM_COUNTS_100("TabGroups.UserCustomizedGroupCountPerLoad",
                           customized_tab_group_count);

  // Record how many windows are open, by type.
  UMA_HISTOGRAM_COUNTS_100("WindowManager.AppWindowCountPerLoad",
                           app_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.PopUpWindowCountPerLoad",
                           popup_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.TabbedWindowCountPerLoad",
                           tabbed_window_count);
}

}  // namespace chrome
