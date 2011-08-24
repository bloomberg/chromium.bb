// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For print preview, a print preview (PP) tab is linked with the initiator tab
// that initiated the printing operation. If the tab initiates a second
// printing operation while the first print preview tab is still open, that PP
// tab is focused/activated. There may be more than one PP tab open. There is a
// 1:1 relationship between PP tabs and initiating tabs. This class manages PP
// tabs and initiator tabs.
#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TAB_CONTROLLER_H_

#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TAB_CONTROLLER_H_
#pragma once

#include <map>

#include "base/memory/ref_counted.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Browser;
class RenderProcessHost;
class TabContents;

namespace content {
struct LoadCommittedDetails;
}

namespace printing {

class PrintPreviewTabController
    : public base::RefCounted<PrintPreviewTabController>,
      public NotificationObserver {
 public:
  PrintPreviewTabController();

  virtual ~PrintPreviewTabController();

  static PrintPreviewTabController* GetInstance();

  // Initiate print preview for |initiator_tab|.
  // Call this instead of GetOrCreatePreviewTab().
  static void PrintPreview(TabContents* initiator_tab);

  // Get/Create the print preview tab for |initiator_tab|.
  // Exposed for unit tests.
  TabContents* GetOrCreatePreviewTab(TabContents* initiator_tab);

  // Returns preview tab for |tab|.
  // Returns |tab| if |tab| is a preview tab.
  // Returns NULL if no preview tab exists for |tab|.
  TabContents* GetPrintPreviewForTab(TabContents* tab) const;

  // Returns initiator tab for |preview_tab|.
  // Returns NULL if no initiator tab exists for |preview_tab|.
  TabContents* GetInitiatorTab(TabContents* preview_tab);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns true if |tab| is a print preview tab.
  static bool IsPrintPreviewTab(TabContents* tab);

  // Erase the initiator tab info associated with |preview_tab|.
  void EraseInitiatorTabInfo(TabContents* preview_tab);

 private:
  friend class base::RefCounted<PrintPreviewTabController>;

  // Handler for the RENDERER_PROCESS_CLOSED notification. This is observed when
  // the initiator renderer crashed.
  void OnRendererProcessClosed(RenderProcessHost* rph);

  // Handler for the TAB_CONTENTS_DESTROYED notification. This is observed when
  // either tab is closed.
  void OnTabContentsDestroyed(TabContents* tab);

  // Handler for the NAV_ENTRY_COMMITTED notification. This is observed when the
  // renderer is navigated to a different page.
  void OnNavEntryCommitted(TabContents* tab,
                           content::LoadCommittedDetails* details);

  // 1:1 relationship between initiator tab and print preview tab.
  // Key: Preview tab.
  // Value: Initiator tab.
  typedef std::map<TabContents*, TabContents*> PrintPreviewTabMap;

  // Creates a new print preview tab.
  TabContents* CreatePrintPreviewTab(TabContents* initiator_tab);

  // Helper function to store the initiator tab(title and url) information
  // in |PrintPreviewUI|.
  void SetInitiatorTabURLAndTitle(TabContents* preview_tab);

  // Adds/Removes observers for notifications from |tab|.
  void AddObservers(TabContents* tab);
  void RemoveObservers(TabContents* tab);

  // Mapping between print preview tab and the corresponding initiator tab.
  PrintPreviewTabMap preview_tab_map_;

  // A registrar for listening notifications.
  NotificationRegistrar registrar_;

  // True if the controller is waiting for a new preview tab via
  // NavigationType::NEW_PAGE.
  bool waiting_for_new_preview_page_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTabController);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TAB_CONTROLLER_H_
