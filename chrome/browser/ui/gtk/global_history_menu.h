// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GLOBAL_HISTORY_MENU_H_
#define CHROME_BROWSER_UI_GTK_GLOBAL_HISTORY_MENU_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/gtk/global_menu_owner.h"
#include "content/browser/cancelable_request.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class Browser;

namespace history {
class TopSites;
}

typedef struct _GdkPixbuf GdkPixbuf;

// Controls the History menu.
class GlobalHistoryMenu : public GlobalMenuOwner,
                          public content::NotificationObserver,
                          public TabRestoreServiceObserver {
 public:
  explicit GlobalHistoryMenu(Browser* browser);
  virtual ~GlobalHistoryMenu();

  // Takes the history menu we need to modify based on the tab restore/most
  // visited state.
  virtual void Init(GtkWidget* history_menu,
                    GtkWidget* history_menu_item) OVERRIDE;

 private:
  class HistoryItem;
  struct ClearMenuClosure;
  struct GetIndexClosure;

  typedef std::map<GtkWidget*, HistoryItem*> MenuItemToHistoryMap;

  // Sends a message off to History for data.
  void GetTopSitesData();

  // Callback to receive data requested from GetTopSitesData().
  void OnTopSitesReceived(const history::MostVisitedURLList& visited_list);

  // Returns the currently existing HistoryItem associated with
  // |menu_item|. Can return NULL.
  HistoryItem* HistoryItemForMenuItem(GtkWidget* menu_item);

  // Returns whether there's a valid HistoryItem representation of |entry|.
  bool HasValidHistoryItemForTab(const TabRestoreService::Tab& entry);

  // Creates a HistoryItem from the data in |entry|.
  HistoryItem* HistoryItemForTab(const TabRestoreService::Tab& entry);

  // Creates a menu item form |item| and inserts it in |menu| at |index|.
  GtkWidget* AddHistoryItemToMenu(HistoryItem* item,
                                  GtkWidget* menu,
                                  int tag,
                                  int index);

  // Find the first index of the item in |menu| with the tag |tag_id|.
  int GetIndexOfMenuItemWithTag(GtkWidget* menu, int tag_id);

  // This will remove all menu items in |menu| with |tag| as their tag. This
  // clears state about HistoryItems* that we keep to prevent that data from
  // going stale. That's why this method recurses into its child menus.
  void ClearMenuSection(GtkWidget* menu, int tag);

  // Implementation detail of GetIndexOfMenuItemWithTag.
  static void GetIndexCallback(GtkWidget* widget, GetIndexClosure* closure);

  // Implementation detail of ClearMenuSection.
  static void ClearMenuCallback(GtkWidget* widget, ClearMenuClosure* closure);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // For TabRestoreServiceObserver
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

  CHROMEGTK_CALLBACK_0(GlobalHistoryMenu, void, OnRecentlyClosedItemActivated);

  // Listen for the first menu show command so we can then connect to the
  // TabRestoreService. With how the global menus work, I'd prefer to register
  // with the TabRestoreService as soon as we're constructed, but this breaks
  // unit tests which test the service (because they force different
  // construction ordering while us connecting to the TabRestoreService loads
  // data now!)
  CHROMEGTK_CALLBACK_0(GlobalHistoryMenu, void, OnMenuActivate);

  Browser* browser_;
  Profile* profile_;

  // The history menu. We keep this since we need to rewrite parts of it
  // periodically.
  ui::OwnedWidgetGtk history_menu_;

  history::TopSites* top_sites_;
  CancelableRequestConsumer top_sites_consumer_;

  TabRestoreService* tab_restore_service_;  // weak

  // A mapping from GtkMenuItems to HistoryItems that maintain data.
  MenuItemToHistoryMap menu_item_history_map_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_GTK_GLOBAL_HISTORY_MENU_H_
