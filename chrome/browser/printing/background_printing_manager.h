// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#pragma once

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class RenderProcessHost;
class TabContentsWrapper;

namespace printing {

// Manages hidden tabs that prints documents in the background.
// The hidden tabs are no longer part of any Browser / TabStripModel.
// They get deleted when the tab finishes printing.
class BackgroundPrintingManager : public base::NonThreadSafe,
                                  public NotificationObserver {
 public:
  typedef std::set<TabContentsWrapper*> TabContentsWrapperSet;

  BackgroundPrintingManager();
  virtual ~BackgroundPrintingManager();

  // Takes ownership of |preview_tab| and deletes it when |preview_tab| finishes
  // printing. This removes the TabContentsWrapper from its TabStrip and
  // hides it from the user.
  void OwnPrintPreviewTab(TabContentsWrapper* preview_tab);

  // Takes ownership of |initiator_tab| and deletes it when its preview tab is
  // destroyed by either being canceled, closed or finishing printing. This
  // removes the TabContentsWrapper from its TabStrip and hides it from the
  // user. Returns true if content has an associated print preview tab,
  // otherwise, returns false and does not take ownership of |initiator_tab|.
  bool OwnInitiatorTab(TabContentsWrapper* initiator_tab);

  // Let others iterate over the list of background printing tabs.
  TabContentsWrapperSet::const_iterator begin();
  TabContentsWrapperSet::const_iterator end();

  // Returns true if |printing_tabs_| contains |preview_tab|.
  bool HasPrintPreviewTab(TabContentsWrapper* preview_tab);

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  typedef std::map<TabContentsWrapper*, TabContentsWrapper*>
      TabContentsWrapperMap;

  // Notifications handlers.
  void OnRendererProcessClosed(RenderProcessHost* rph);
  void OnPrintJobReleased(TabContentsWrapper* preview_tab);
  void OnTabContentsDestroyed(TabContentsWrapper* preview_tab);

  // Removes |tab| from its tab strip.
  void RemoveFromTabStrip(TabContentsWrapper* tab);

  // The set of print preview tabs managed by BackgroundPrintingManager.
  TabContentsWrapperSet printing_tabs_;

  // 1:1 mapping between an initiator tab managed by BackgroundPrintingManager
  // and its associated print preview tab. The print preview tab need not be in
  // |printing_tabs_|.
  // Key: print preview tab.
  // Value: initiator tab.
  TabContentsWrapperMap map_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPrintingManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
