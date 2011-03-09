// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
#pragma once

#import <Cocoa/Cocoa.h>
#include <map>

#include "base/ref_counted.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "content/browser/cancelable_request.h"
#include "content/common/notification_observer.h"

class NavigationEntry;
class NotificationRegistrar;
class PageUsageData;
class Profile;
class TabNavigationEntry;
class TabRestoreService;
@class HistoryMenuCocoaController;

namespace {

class HistoryMenuBridgeTest;

}

// C++ bridge for the history menu; one per AppController (means there
// is only one). This class observes various data sources, namely the
// HistoryService and the TabRestoreService, and then updates the NSMenu when
// there is new data.
//
// The history menu is broken up into sections: most visisted and recently
// closed. The overall menu has a tag of IDC_HISTORY_MENU, with the user content
// items having the local tags defined in the enum below. Items within a section
// all share the same tag. The structure of the menu is laid out in MainMenu.xib
// and the generated content is inserted after the Title elements. The recently
// closed section is special in that those menu items can have submenus to list
// all the tabs within that closed window. By convention, these submenu items
// have a tag that's equal to the parent + 1. Tags within the history menu have
// a range of [400,500) and do not go through CommandDispatch for their target-
// action mechanism.
//
// These menu items do not use firstResponder as their target. Rather, they are
// hooked directly up to the HistoryMenuCocoaController that then bridges back
// to this class. These items are created via the AddItemToMenu() helper. Also,
// unlike the typical ownership model, this bridge owns its controller. The
// controller is very thin and only exists to interact with Cocoa, but this
// class does the bulk of the work.
class HistoryMenuBridge : public NotificationObserver,
                          public TabRestoreServiceObserver {
 public:
  // This is a generalization of the data we store in the history menu because
  // we pull things from different sources with different data types.
  struct HistoryItem {
   public:
    HistoryItem();
    // Copy constructor allowed.
    HistoryItem(const HistoryItem& copy);
    ~HistoryItem();

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

    // The pointer to the item after it has been created. Strong; NSMenu also
    // retains this. During a rebuild flood (if the user closes a lot of tabs
    // quickly), the NSMenu can release the item before the HistoryItem has
    // been fully deleted. If this were a weak pointer, it would result in a
    // zombie.
    scoped_nsobject<NSMenuItem> menu_item;

    // This ID is unique for a browser session and can be passed to the
    // TabRestoreService to re-open the closed window or tab that this
    // references. A non-0 session ID indicates that this is an entry can be
    // restored that way. Otherwise, the URL will be used to open the item and
    // this ID will be 0.
    SessionID::id_type session_id;

    // If the HistoryItem is a window, this will be the vector of tabs. Note
    // that this is a list of weak references. The |menu_item_map_| is the owner
    // of all items. If it is not a window, then the entry is a single page and
    // the vector will be empty.
    std::vector<HistoryItem*> tabs;

   private:
    // Copying is explicitly allowed, but assignment is not.
    void operator=(const HistoryItem&);
  };

  // These tags are not global view tags and are local to the history menu. The
  // normal procedure for menu items is to go through CommandDispatch, but since
  // history menu items are hooked directly up to their target, they do not need
  // to have the global IDC view tags.
  enum Tags {
    kMostVisitedSeparator = 400,  // Separator before most visited section.
    kMostVisitedTitle = 401,  // Title of the most visited section.
    kMostVisited = 420,  // Used for all entries in the most visited section.
    kRecentlyClosedSeparator = 440,  // Item before recently closed section.
    kRecentlyClosedTitle = 441,  // Title of recently closed section.
    kRecentlyClosed = 460,  // Used for items in the recently closed section.
    kShowFullSeparator = 480  // Separator after the recently closed section.
  };

  explicit HistoryMenuBridge(Profile* profile);
  virtual ~HistoryMenuBridge();

  // Overriden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // For TabRestoreServiceObserver
  virtual void TabRestoreServiceChanged(TabRestoreService* service);
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

  // Looks up an NSMenuItem in the |menu_item_map_| and returns the
  // corresponding HistoryItem.
  HistoryItem* HistoryItemForMenuItem(NSMenuItem* item);

  // I wish I has a "friend @class" construct. These are used by the HMCC
  // to access model information when responding to actions.
  HistoryService* service();
  Profile* profile();

 protected:
  // Return the History menu.
  virtual NSMenu* HistoryMenu();

  // Clear items in the given |menu|. Menu items in the same section are given
  // the same tag. This will go through the entire history menu, removing all
  // items with a given tag. Note that this will recurse to submenus, removing
  // child items from the menu item map. This will only remove items that have
  // a target hooked up to the |controller_|.
  void ClearMenuSection(NSMenu* menu, NSInteger tag);

  // Adds a given title and URL to the passed-in menu with a certain tag and
  // index. This will add |item| and the newly created menu item to the
  // |menu_item_map_|, which takes ownership. Items are deleted in
  // ClearMenuSection(). This returns the new menu item that was just added.
  NSMenuItem* AddItemToMenu(HistoryItem* item,
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

  // Creates a HistoryItem* for the given tab entry. Caller takes ownership of
  // the result and must delete it when finished.
  HistoryItem* HistoryItemForTab(const TabRestoreService::Tab& entry);

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
  friend class ::HistoryMenuBridgeTest;
  friend class HistoryMenuCocoaControllerTest;

  scoped_nsobject<HistoryMenuCocoaController> controller_;  // strong

  Profile* profile_;  // weak
  HistoryService* history_service_;  // weak
  TabRestoreService* tab_restore_service_;  // weak

  NotificationRegistrar registrar_;
  CancelableRequestConsumer cancelable_request_consumer_;

  // Mapping of NSMenuItems to HistoryItems. This owns the HistoryItems until
  // they are removed and deleted via ClearMenuSection().
  std::map<NSMenuItem*, HistoryItem*> menu_item_map_;

  // Maps HistoryItems to favicon request Handles.
  CancelableRequestConsumerTSimple<HistoryItem*> favicon_consumer_;

  // Requests to re-create the menu are coalesced. |create_in_progress_| is true
  // when either waiting for the history service to return query results, or
  // when the menu is rebuilding. |need_recreate_| is true whenever a rebuild
  // has been scheduled but is waiting for the current one to finish.
  bool create_in_progress_;
  bool need_recreate_;

  // The default favicon if a HistoryItem does not have one.
  scoped_nsobject<NSImage> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(HistoryMenuBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
