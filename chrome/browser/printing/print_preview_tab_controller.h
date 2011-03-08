// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "base/ref_counted.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class TabContents;

namespace printing {

class PrintPreviewTabController
    : public base::RefCounted<PrintPreviewTabController>,
      public NotificationObserver {
 public:
  PrintPreviewTabController();

  virtual ~PrintPreviewTabController();

  static PrintPreviewTabController* GetInstance();

  // Get/Create the print preview tab for |initiator_tab|.
  // |browser_window_id| is the browser window containing |initiator_tab|.
  TabContents* GetOrCreatePreviewTab(
      TabContents* initiator_tab, int browser_window_id);

  // Returns preview tab for |tab|.
  // Returns |tab| if |tab| is a preview tab.
  // Returns NULL if no preview tab exists for |tab|.
  TabContents* GetPrintPreviewForTab(TabContents* tab) const;

  // Returns initiator tab for |preview_tab|.
  // Returns NULL if no initiator tab exists for |preview_tab|.
  TabContents* GetInitiatorTab(TabContents* preview_tab);

  // Notification observer implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns true if |tab| is a print preview tab.
  static bool IsPrintPreviewTab(TabContents* tab);

 private:
  friend class base::RefCounted<PrintPreviewTabController>;

  // 1:1 relationship between initiator tab and print preview tab.
  // Key: Preview tab.
  // Value: Initiator tab.
  typedef std::map<TabContents*, TabContents*> PrintPreviewTabMap;

  // Creates a new print preview tab.
  TabContents* CreatePrintPreviewTab(
      TabContents* initiator_tab, int browser_window_id);

  // Adds/Removes observers for notifications from |tab|.
  void AddObservers(TabContents* tab);
  void RemoveObservers(TabContents* tab);

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
