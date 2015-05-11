// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_

#include <set>

#include "base/callback_list.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_widget_host.h"

namespace content {
class NavigationController;
}

// SessionRestoreStatsCollector observes SessionRestore events ands records UMA
// accordingly.
class SessionRestoreStatsCollector
    : public content::NotificationObserver,
      public base::RefCounted<SessionRestoreStatsCollector> {
 public:
  // Called to start tracking tabs. If a restore is already occuring, the tabs
  // are added to the existing list of tracked tabs.
  static void TrackTabs(
      const std::vector<SessionRestoreDelegate::RestoredTab>& tabs,
      const base::TimeTicks& restore_started);

  // Called to start tracking only active tabs. If a restore is already
  // occuring, the tabs are added to the existing list of tracked tabs.
  static void TrackActiveTabs(
      const std::vector<SessionRestoreDelegate::RestoredTab>& tabs,
      const base::TimeTicks& restore_started);

 private:
  friend class base::RefCounted<SessionRestoreStatsCollector>;

  using RenderWidgetHostSet = std::set<content::RenderWidgetHost*>;

  explicit SessionRestoreStatsCollector(const base::TimeTicks& restore_started);
  ~SessionRestoreStatsCollector() override;

  // NotificationObserver method.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Adds new tabs to the list of tracked tabs.
  void AddTabs(const std::vector<SessionRestoreDelegate::RestoredTab>& tabs);

  // Called when a tab is no longer tracked.
  void RemoveTab(content::NavigationController* tab);

  // Registers for relevant notifications for a tab.
  void RegisterForNotifications(content::NavigationController* tab);

  // Returns the RenderWidgetHost of a tab.
  content::RenderWidgetHost* GetRenderWidgetHost(
      content::NavigationController* tab);

  // Have we recorded the times for a foreground tab load?
  bool got_first_foreground_load_;

  // Have we recorded the times for a foreground tab paint?
  bool got_first_paint_;

  // The time the restore process started.
  base::TimeTicks restore_started_;

  // The renderers we have started loading into.
  RenderWidgetHostSet render_widget_hosts_loading_;

  // The renderers we have loaded and are waiting on to paint.
  RenderWidgetHostSet render_widget_hosts_to_paint_;

  // List of tracked tabs.
  std::set<content::NavigationController*> tabs_tracked_;

  // The number of tabs that have been restored.
  int tab_count_;

  // Max number of tabs that were loaded in parallel (for metrics).
  size_t max_parallel_tab_loads_;

  // Notification registrar.
  content::NotificationRegistrar registrar_;

  // To keep the collector alive as long as needed.
  scoped_refptr<SessionRestoreStatsCollector> this_retainer_;

  // The shared SessionRestoreNotifier instance for all SessionRestores running
  // at this time.
  static SessionRestoreStatsCollector* shared_collector_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreStatsCollector);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
