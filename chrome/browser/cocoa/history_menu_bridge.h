// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C++ controller for the history menu; one per AppController (means there
// is only one). This class observes various data sources, namely the
// HistoryService and the TabRestoreService, and then updates the NSMenu when
// there is new data.
//
// The history menu is broken up into sections: most visisted and recently
// closed. The overall menu has a tag of IDC_HISTORY_MENU, with two sections
// IDC_HISTORY_MENU_VISITED and IDC_HISTORY_MENU_CLOSED, which are used to
// delineate the two sections. Items within these sections are assigned tags
// within IDC_HISTORY_MENU_* + 1..99. Most Chromium Cocoa menu items are static
// from a nib (e.g. New Tab), but may be enabled/disabled under certain
// circumstances (e.g. Cut and Paste). In addition, most Cocoa menu items have
// firstResponder: as a target. Unusually, bookmark menu items are created
// dynamically. They also have a target of HistoryMenuCocoaController, not
// firstResponder. See HistoryMenuBridge::AddItemToMenu()). Unlike most of our
// Cocoa-Bridge classes, the HMCC is not at the root of the ownership model
// because its only function is to respond to menu item actions; everything
// else is done in this bridge.

#ifndef CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_
#define CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/common/notification_observer.h"

class NavigationEntry;
class NotificationRegistrar;
class PageUsageData;
class Profile;
class TabNavigationEntry;
@class HistoryMenuCocoaController;

class HistoryMenuBridge : public NotificationObserver,
                          public TabRestoreService::Observer {
 public:
  // This is a generalization of the data we store in the history menu because
  // we pull things from different sources with different data types.
  struct HistoryItem {
    HistoryItem() {}
    ~HistoryItem() {}

    string16 title;
    GURL url;
  };

  HistoryMenuBridge(Profile* profile);
  virtual ~HistoryMenuBridge();

  // Overriden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // For TabRestoreService::Observer
  virtual void TabRestoreServiceChanged(TabRestoreService* service);
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

  // I wish I has a "friend @class" construct. These are used by the HMCC
  // to access model information when responding to actions.
  HistoryService* service();
  Profile* profile();
  std::vector<HistoryItem>* visited_results();
  std::vector<HistoryItem>* closed_results();

 protected:
  // Return the History menu.
  NSMenu* HistoryMenu();

  // Clear items in the given |menu|. The menu is broken into sections, defined
  // by IDC_HISTORY_MENU_* constants. This function will clear |count| menu
  // items, starting from |tag|.
  void ClearMenuSection(NSMenu* menu, NSInteger tag, unsigned int count);

  // Adds a given title and URL to HistoryMenu() with a certain tag and index.
  void AddItemToMenu(const HistoryItem& item,
                     NSMenu* menu,
                     NSInteger tag,
                     NSInteger index);

  // Called by the ctor if |service_| is ready at the time, or by a
  // notification receiver. Finishes initialization tasks by subscribing for
  // change notifications and calling CreateMenu().
  void Init();

  // Does the query for the history information to create the menu.
  void CreateMenu();

  // Callback method for when HistoryService query results are ready with the
  // most recently-visited sites.
  void OnVisitedHistoryResults(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* results);

  // Tries to add the current Tab's TabNavigationEntry's NavigationEntry object
  // to |closed_results_|. Return TRUE if the operation completed successfully.
  bool AddNavigationForTab(const TabRestoreService::Tab& entry);

 private:
  friend class HistoryMenuBridgeTest;

  scoped_nsobject<HistoryMenuCocoaController> controller_;  // strong

  Profile* profile_; // weak
  HistoryService* history_service_; // weak
  TabRestoreService* tab_restore_service_; // weak

  NotificationRegistrar registrar_;
  CancelableRequestConsumer cancelable_request_consumer_;

  // The most recent results we've received.
  std::vector<HistoryItem> visited_results_;
  std::vector<HistoryItem> closed_results_;

  // We coalesce requests to re-create the menu. |create_in_progress_| is true
  // whenever we are either waiting for the history service to return query
  // results, or when we are rebuilding. |need_recreate_| is true whenever a
  // rebuild has been scheduled but is waiting for the current one to finish.
  bool create_in_progress_;
  bool need_recreate_;

  DISALLOW_COPY_AND_ASSIGN(HistoryMenuBridge);
};

#endif  // CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_
