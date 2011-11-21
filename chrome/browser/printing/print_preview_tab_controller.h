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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class TabContentsWrapper;

namespace content {
struct LoadCommittedDetails;
class RenderProcessHost;
}

namespace printing {

class PrintPreviewTabController
    : public base::RefCounted<PrintPreviewTabController>,
      public content::NotificationObserver {
 public:
  PrintPreviewTabController();

  virtual ~PrintPreviewTabController();

  static PrintPreviewTabController* GetInstance();

  // Initiate print preview for |initiator_tab|.
  // Call this instead of GetOrCreatePreviewTab().
  static void PrintPreview(TabContentsWrapper* initiator_tab);

  // Get/Create the print preview tab for |initiator_tab|.
  // Exposed for unit tests.
  TabContentsWrapper* GetOrCreatePreviewTab(TabContentsWrapper* initiator_tab);

  // Returns preview tab for |tab|.
  // Returns |tab| if |tab| is a preview tab.
  // Returns NULL if no preview tab exists for |tab|.
  TabContentsWrapper* GetPrintPreviewForTab(TabContentsWrapper* tab) const;

  // Returns initiator tab for |preview_tab|.
  // Returns NULL if no initiator tab exists for |preview_tab|.
  TabContentsWrapper* GetInitiatorTab(TabContentsWrapper* preview_tab);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if |tab| is a print preview tab.
  static bool IsPrintPreviewTab(TabContentsWrapper* tab);

  // Returns true if |url| is a print preview url.
  static bool IsPrintPreviewURL(const GURL& url);

  // Erase the initiator tab info associated with |preview_tab|.
  void EraseInitiatorTabInfo(TabContentsWrapper* preview_tab);

  bool is_creating_print_preview_tab() const;

 private:
  friend class base::RefCounted<PrintPreviewTabController>;

  // 1:1 relationship between initiator tab and print preview tab.
  // Key: Preview tab.
  // Value: Initiator tab.
  typedef std::map<TabContentsWrapper*, TabContentsWrapper*> PrintPreviewTabMap;

  // Handler for the RENDERER_PROCESS_CLOSED notification. This is observed when
  // the initiator renderer crashed.
  void OnRendererProcessClosed(content::RenderProcessHost* rph);

  // Handler for the TAB_CONTENTS_DESTROYED notification. This is observed when
  // either tab is closed.
  void OnTabContentsDestroyed(TabContentsWrapper* tab);

  // Handler for the NAV_ENTRY_COMMITTED notification. This is observed when the
  // renderer is navigated to a different page.
  void OnNavEntryCommitted(TabContentsWrapper* tab,
                           content::LoadCommittedDetails* details);

  // Creates a new print preview tab.
  TabContentsWrapper* CreatePrintPreviewTab(TabContentsWrapper* initiator_tab);

  // Helper function to store the initiator tab(title and url) information
  // in PrintPreviewUI.
  void SetInitiatorTabURLAndTitle(TabContentsWrapper* preview_tab);

  // Adds/Removes observers for notifications from |tab|.
  void AddObservers(TabContentsWrapper* tab);
  void RemoveObservers(TabContentsWrapper* tab);

  // Removes tabs when they close/crash/navigate.
  void RemoveInitiatorTab(TabContentsWrapper* initiator_tab);
  void RemovePreviewTab(TabContentsWrapper* preview_tab);

  // Mapping between print preview tab and the corresponding initiator tab.
  PrintPreviewTabMap preview_tab_map_;

  // A registrar for listening notifications.
  content::NotificationRegistrar registrar_;

  // True if the controller is waiting for a new preview tab via
  // content::NAVIGATION_TYPE_NEW_PAGE.
  bool waiting_for_new_preview_page_;

  // Whether the PrintPreviewTabController is in the middle of creating a
  // print preview tab.
  bool is_creating_print_preview_tab_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTabController);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TAB_CONTROLLER_H_
