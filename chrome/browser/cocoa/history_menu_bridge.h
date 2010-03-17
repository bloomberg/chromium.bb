// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_
#define CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/cancelable_request.h"
#import "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/common/notification_observer.h"

class NavigationEntry;
class NotificationRegistrar;
class PageUsageData;
class Profile;
class TabNavigationEntry;
@class HistoryMenuCocoaController;

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
// firstResponder: as a target. Unusually, history menu items are created
// dynamically. They also have a target of HistoryMenuCocoaController, not
// firstResponder. See HistoryMenuBridge::AddItemToMenu(). Unlike most of our
// Cocoa-Bridge classes, the HMCC is not at the root of the ownership model
// because its only function is to respond to menu item actions; everything
// else is done in this bridge.
class HistoryMenuBridge : public NotificationObserver,
                          public TabRestoreService::Observer {
 public:
  // This is a generalization of the data we store in the history menu because
  // we pull things from different sources with different data types.
  struct HistoryItem {
   public:
    HistoryItem() : icon_requested(false) {}
    ~HistoryItem() {}

    // The title for the menu item.
    string16 title;
    // The URL that will be navigated to if the user selects this item.
    GURL url;
    // Favicon for the URL.
    scoped_nsobject<NSImage> icon;

    // If the icon is being requested from the FaviconService, |icon_requested|
    // will be true and |icon_handle| will be non-NULL. If this is false, then
    // |icon_handle| will be NULL.
    bool icon_requested;
    // The Handle given to us by the FaviconService for the icon fetch request.
    FaviconService::Handle icon_handle;

    // The pointer to the item after it has been created. Weak.
    NSMenuItem* menu_item;

   private:
    DISALLOW_COPY_AND_ASSIGN(HistoryItem);
  };

  explicit HistoryMenuBridge(Profile* profile);
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
  const ScopedVector<HistoryItem>* const visited_results();
  const ScopedVector<HistoryItem>* const closed_results();

 protected:
  // Return the History menu.
  NSMenu* HistoryMenu();

  // Clear items in the given |menu|. The menu is broken into sections, defined
  // by IDC_HISTORY_MENU_* constants. This function will clear |count| menu
  // items, starting from |tag|.
  void ClearMenuSection(NSMenu* menu, NSInteger tag, unsigned int count);

  // Adds a given title and URL to HistoryMenu() with a certain tag and index.
  void AddItemToMenu(HistoryItem* item,
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

  // Helper function that sends an async request to the FaviconService to get
  // an icon. The callback will update the NSMenuItem directly.
  void GetFaviconForHistoryItem(HistoryItem* item);

  // Callback for the FaviconService to return favicon image data when we
  // request it. This decodes the raw data, updates the HistoryItem, and then
  // sets the image on the menu. Called on the same same thread that
  // GetFaviconForHistoryItem() was called on (UI thread).
  void GotFaviconData(FaviconService::Handle handle,
                      bool know_favicon,
                      scoped_refptr<RefCountedMemory> data,
                      bool expired,
                      GURL url);

  // Cancels a favicon load request for a given HistoryItem, if one is in
  // progress.
  void CancelFaviconRequest(HistoryItem* item);

 private:
  friend class HistoryMenuBridgeTest;

  scoped_nsobject<HistoryMenuCocoaController> controller_;  // strong

  Profile* profile_; // weak
  HistoryService* history_service_; // weak
  TabRestoreService* tab_restore_service_; // weak

  NotificationRegistrar registrar_;
  CancelableRequestConsumer cancelable_request_consumer_;

  // The most recent results we've received.
  ScopedVector<HistoryItem> visited_results_;
  ScopedVector<HistoryItem> closed_results_;

  // Maps HistoryItems to favicon request Handles.
  CancelableRequestConsumerTSimple<HistoryItem*> favicon_consumer_;

  // We coalesce requests to re-create the menu. |create_in_progress_| is true
  // whenever we are either waiting for the history service to return query
  // results, or when we are rebuilding. |need_recreate_| is true whenever a
  // rebuild has been scheduled but is waiting for the current one to finish.
  bool create_in_progress_;
  bool need_recreate_;

  // The default favicon if a HistoryItem does not have one.
  scoped_nsobject<NSImage> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(HistoryMenuBridge);
};

#endif  // CHROME_BROWSER_COCOA_HISTORY_MENU_BRIDGE_H_
