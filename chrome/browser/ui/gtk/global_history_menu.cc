// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_history_menu.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/gtk/global_menu_bar.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gtk_util.h"

namespace {

// The maximum number of most visited items to display.
const unsigned int kMostVisitedCount = 8;

// The number of recently closed items to get.
const unsigned int kRecentlyClosedCount = 8;

// Menus more than this many chars long will get trimmed.
const int kMaximumMenuWidthInChars = 50;

}  // namespace

struct GlobalHistoryMenu::ClearMenuClosure {
  GtkWidget* container;
  GlobalHistoryMenu* menu_bar;
  int tag;
};

struct GlobalHistoryMenu::GetIndexClosure {
  bool found;
  int current_index;
  int tag;
};

class GlobalHistoryMenu::HistoryItem {
 public:
  HistoryItem()
      : menu_item(NULL),
        session_id(0) {}

  // The title for the menu item.
  string16 title;
  // The URL that will be navigated to if the user selects this item.
  GURL url;

  // A pointer to the menu_item. This is a weak reference in the GTK+ version
  // because the GtkMenu must sink the reference.
  GtkWidget* menu_item;

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
  DISALLOW_COPY_AND_ASSIGN(HistoryItem);
};

GlobalHistoryMenu::GlobalHistoryMenu(Browser* browser)
    : browser_(browser),
      profile_(browser_->profile()),
      top_sites_(NULL),
      tab_restore_service_(NULL) {
}

GlobalHistoryMenu::~GlobalHistoryMenu() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);

  STLDeleteContainerPairSecondPointers(menu_item_history_map_.begin(),
                                       menu_item_history_map_.end());
  menu_item_history_map_.clear();
}

void GlobalHistoryMenu::Init(GtkWidget* history_menu,
                             GtkWidget* history_menu_item) {
  history_menu_.Own(history_menu);

  // We have to connect to |history_menu_item|'s "activate" signal instead of
  // |history_menu|'s "show" signal because we are not supposed to modify the
  // menu during "show"
  g_signal_connect(history_menu_item, "activate",
                   G_CALLBACK(OnMenuActivateThunk), this);

  if (profile_) {
    top_sites_ = profile_->GetTopSites();
    if (top_sites_) {
      GetTopSitesData();

      // Register for notification when TopSites changes so that we can update
      // ourself.
      registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                     content::Source<history::TopSites>(top_sites_));
    }
  }
}

void GlobalHistoryMenu::GetTopSitesData() {
  DCHECK(top_sites_);

  top_sites_->GetMostVisitedURLs(
      &top_sites_consumer_,
      base::Bind(&GlobalHistoryMenu::OnTopSitesReceived,
                 base::Unretained(this)));
}

void GlobalHistoryMenu::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  ClearMenuSection(history_menu_.get(), GlobalMenuBar::TAG_MOST_VISITED);

  int index = GetIndexOfMenuItemWithTag(
      history_menu_.get(),
      GlobalMenuBar::TAG_MOST_VISITED_HEADER) + 1;

  for (size_t i = 0; i < visited_list.size() && i < kMostVisitedCount; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.

    HistoryItem* item = new HistoryItem();
    item->title = visited.title;
    item->url = visited.url;

    AddHistoryItemToMenu(item,
                         history_menu_.get(),
                         GlobalMenuBar::TAG_MOST_VISITED,
                         index++);
  }
}

GlobalHistoryMenu::HistoryItem* GlobalHistoryMenu::HistoryItemForMenuItem(
    GtkWidget* menu_item) {
  MenuItemToHistoryMap::iterator it = menu_item_history_map_.find(menu_item);
  return it != menu_item_history_map_.end() ? it->second : NULL;
}

GlobalHistoryMenu::HistoryItem* GlobalHistoryMenu::HistoryItemForTab(
    const TabRestoreService::Tab& entry) {
  const TabNavigation& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  HistoryItem* item = new HistoryItem();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;

  return item;
}

GtkWidget* GlobalHistoryMenu::AddHistoryItemToMenu(HistoryItem* item,
                                                   GtkWidget* menu,
                                                   int tag,
                                                   int index) {
  string16 title = item->title;
  std::string url_string = item->url.possibly_invalid_spec();

  if (title.empty())
    title = UTF8ToUTF16(url_string);
  ui::ElideString(title, kMaximumMenuWidthInChars, &title);

  GtkWidget* menu_item = gtk_menu_item_new_with_label(
      UTF16ToUTF8(title).c_str());

  item->menu_item = menu_item;
  gtk_widget_show(menu_item);
  g_object_set_data(G_OBJECT(menu_item), "type-tag", GINT_TO_POINTER(tag));
  g_signal_connect(menu_item, "activate",
                   G_CALLBACK(OnRecentlyClosedItemActivatedThunk), this);

  std::string tooltip = gtk_util::BuildTooltipTitleFor(item->title, item->url);
  gtk_widget_set_tooltip_markup(menu_item, tooltip.c_str());

  menu_item_history_map_.insert(std::make_pair(menu_item, item));
  gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menu_item, index);

  return menu_item;
}

int GlobalHistoryMenu::GetIndexOfMenuItemWithTag(GtkWidget* menu, int tag_id) {
  GetIndexClosure closure;
  closure.found = false;
  closure.current_index = 0;
  closure.tag = tag_id;

  gtk_container_foreach(
      GTK_CONTAINER(menu),
      reinterpret_cast<void (*)(GtkWidget*, void*)>(GetIndexCallback),
      &closure);

  return closure.current_index;
}

void GlobalHistoryMenu::ClearMenuSection(GtkWidget* menu, int tag) {
  ClearMenuClosure closure;
  closure.container = menu;
  closure.menu_bar = this;
  closure.tag = tag;

  gtk_container_foreach(
      GTK_CONTAINER(menu),
      reinterpret_cast<void (*)(GtkWidget*, void*)>(ClearMenuCallback),
      &closure);
}

// static
void GlobalHistoryMenu::GetIndexCallback(GtkWidget* menu_item,
                                         GetIndexClosure* closure) {
  int tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "type-tag"));
  if (tag == closure->tag)
    closure->found = true;

  if (!closure->found)
    closure->current_index++;
}

// static
void GlobalHistoryMenu::ClearMenuCallback(GtkWidget* menu_item,
                                          ClearMenuClosure* closure) {
  DCHECK_NE(closure->tag, 0);

  int tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "type-tag"));
  if (closure->tag == tag) {
    HistoryItem* item = closure->menu_bar->HistoryItemForMenuItem(menu_item);

    if (item) {
      closure->menu_bar->menu_item_history_map_.erase(menu_item);
      delete item;
    }

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
    if (submenu)
      closure->menu_bar->ClearMenuSection(submenu, closure->tag);

    gtk_container_remove(GTK_CONTAINER(closure->container), menu_item);
  }
}

void GlobalHistoryMenu::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TOP_SITES_CHANGED) {
    GetTopSitesData();
  } else {
    NOTREACHED();
  }
}

void GlobalHistoryMenu::TabRestoreServiceChanged(TabRestoreService* service) {
  const TabRestoreService::Entries& entries = service->entries();

  ClearMenuSection(history_menu_.get(), GlobalMenuBar::TAG_RECENTLY_CLOSED);

  // We'll get the index the "Recently Closed" header. (This can vary depending
  // on the number of "Most Visited" items.
  int index = GetIndexOfMenuItemWithTag(
      history_menu_.get(),
      GlobalMenuBar::TAG_RECENTLY_CLOSED_HEADER) + 1;

  unsigned int added_count = 0;
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < kRecentlyClosedCount; ++it) {
    TabRestoreService::Entry* entry = *it;

    if (entry->type == TabRestoreService::WINDOW) {
      TabRestoreService::Window* entry_win =
          static_cast<TabRestoreService::Window*>(entry);
      std::vector<TabRestoreService::Tab>& tabs = entry_win->tabs;
      if (tabs.empty())
        continue;

      // Create the item for the parent/window.
      HistoryItem* item = new HistoryItem();
      item->session_id = entry_win->id;

      GtkWidget* submenu = gtk_menu_new();
      GtkWidget* restore_item = gtk_menu_item_new_with_label(
          l10n_util::GetStringUTF8(
              IDS_HISTORY_CLOSED_RESTORE_WINDOW_LINUX).c_str());
      g_object_set_data(G_OBJECT(restore_item), "type-tag",
                        GINT_TO_POINTER(GlobalMenuBar::TAG_RECENTLY_CLOSED));
      g_signal_connect(restore_item, "activate",
                       G_CALLBACK(OnRecentlyClosedItemActivatedThunk), this);
      gtk_widget_show(restore_item);

      // The mac version of this code allows the user to click on the parent
      // menu item to have the same effect as clicking the restore window
      // submenu item. GTK+ helpfully activates a menu item when it shows a
      // submenu so toss that feature out.
      menu_item_history_map_.insert(std::make_pair(restore_item, item));
      gtk_menu_shell_append(GTK_MENU_SHELL(submenu), restore_item);

      GtkWidget* separator = gtk_separator_menu_item_new();
      gtk_widget_show(separator);
      gtk_menu_shell_append(GTK_MENU_SHELL(submenu), separator);

      // Loop over the window's tabs and add them to the submenu.
      int subindex = 2;
      std::vector<TabRestoreService::Tab>::const_iterator iter;
      for (iter = tabs.begin(); iter != tabs.end(); ++iter) {
        TabRestoreService::Tab tab = *iter;
        HistoryItem* tab_item = HistoryItemForTab(tab);
        item->tabs.push_back(tab_item);
        AddHistoryItemToMenu(tab_item,
                             submenu,
                             GlobalMenuBar::TAG_RECENTLY_CLOSED,
                             subindex++);
      }

      std::string title = item->tabs.size() == 1 ?
          l10n_util::GetStringUTF8(
              IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE) :
          l10n_util::GetStringFUTF8(
              IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE,
              base::IntToString16(item->tabs.size()));

      // Create the menu item parent. Unlike mac, it's can't be activated.
      GtkWidget* parent_item = gtk_menu_item_new_with_label(title.c_str());
      gtk_widget_show(parent_item);
      g_object_set_data(G_OBJECT(parent_item), "type-tag",
                        GINT_TO_POINTER(GlobalMenuBar::TAG_RECENTLY_CLOSED));
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent_item), submenu);

      gtk_menu_shell_insert(GTK_MENU_SHELL(history_menu_.get()), parent_item,
                            index++);
      ++added_count;
    } else if (entry->type == TabRestoreService::TAB) {
      TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
      HistoryItem* item = HistoryItemForTab(*tab);
      AddHistoryItemToMenu(item,
                           history_menu_.get(),
                           GlobalMenuBar::TAG_RECENTLY_CLOSED,
                           index++);
      ++added_count;
    }
  }
}

void GlobalHistoryMenu::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

void GlobalHistoryMenu::OnRecentlyClosedItemActivated(GtkWidget* sender) {
  WindowOpenDisposition disposition =
      gtk_util::DispositionForCurrentButtonPressEvent();
  HistoryItem* item = HistoryItemForMenuItem(sender);

  // If this item can be restored using TabRestoreService, do so. Otherwise,
  // just load the URL.
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (item->session_id && service) {
    service->RestoreEntryById(browser_->tab_restore_service_delegate(),
                              item->session_id, UNKNOWN);
  } else {
    DCHECK(item->url.is_valid());
    browser_->OpenURL(OpenURLParams(item->url, content::Referrer(), disposition,
                      content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  }
}

void GlobalHistoryMenu::OnMenuActivate(GtkWidget* sender) {
  if (!tab_restore_service_) {
    tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service_) {
      tab_restore_service_->LoadTabsFromLastSession();
      tab_restore_service_->AddObserver(this);

      // If LoadTabsFromLastSession doesn't load tabs, it won't call
      // TabRestoreServiceChanged(). This ensures that all new windows after
      // the first one will have their menus populated correctly.
      TabRestoreServiceChanged(tab_restore_service_);
    }
  }
}
