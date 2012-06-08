// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#pragma once

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TabContents;

namespace content {
class RenderProcessHost;
}

namespace printing {

// Manages hidden tabs that prints documents in the background.
// The hidden tabs are no longer part of any Browser / TabStripModel.
// They get deleted when the tab finishes printing.
class BackgroundPrintingManager : public base::NonThreadSafe,
                                  public content::NotificationObserver {
 public:
  typedef std::set<TabContents*> TabContentsSet;

  BackgroundPrintingManager();
  virtual ~BackgroundPrintingManager();

  // Takes ownership of |preview_tab| and deletes it when |preview_tab| finishes
  // printing. This removes the TabContents from its TabStrip and hides it from
  // the user.
  void OwnPrintPreviewTab(TabContents* preview_tab);

  // Let others iterate over the list of background printing tabs.
  TabContentsSet::const_iterator begin();
  TabContentsSet::const_iterator end();

  // Returns true if |printing_tabs_| contains |preview_tab|.
  bool HasPrintPreviewTab(TabContents* preview_tab);

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Notifications handlers.
  void OnRendererProcessClosed(content::RenderProcessHost* rph);
  void OnPrintJobReleased(TabContents* preview_tab);
  void OnTabContentsDestroyed(TabContents* preview_tab);

  // Add |tab| to the pending deletion set and schedule deletion.
  void DeletePreviewTab(TabContents* tab);

  // Check if any of the TabContents in |set| share a RenderProcessHost
  // with |tab|, excluding |tab|.
  bool HasSharedRenderProcessHost(const TabContentsSet& set,
                                  TabContents* tab);

  // The set of print preview tabs managed by BackgroundPrintingManager.
  TabContentsSet printing_tabs_;

  // The set of print preview tabs managed by BackgroundPrintingManager that
  // are pending deletion.
  TabContentsSet printing_tabs_pending_deletion_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPrintingManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
