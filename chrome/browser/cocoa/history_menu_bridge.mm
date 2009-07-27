// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/history_menu_bridge.h"
#include "base/sys_string_conversions.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_HISTORY_MENU
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/cocoa/history_menu_cocoa_controller.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"

namespace {

// Menus more than this many chars long will get trimmed.
const static NSUInteger kMaximumMenuWidthInChars = 65;

// When trimming, use this many chars from each side.
const static NSUInteger kMenuTrimSizeInChars = 30;

// Number of days to consider when getting the number of most visited items.
const static int kMostVisitedScope = 90;

// The number of most visisted results to get.
const static int kMostVisitedCount = 9;

// The number of recently closed items to get.
const static unsigned int kRecentlyClosedCount = 4;

}

HistoryMenuBridge::HistoryMenuBridge(Profile* profile)
    : controller_([[HistoryMenuCocoaController alloc] initWithBridge:this]),
      profile_(profile),
      history_service_(NULL),
      tab_restore_service_(NULL),
      create_in_progress_(false),
      need_recreate_(false) {
  // If we don't have a profile, do not bother initializing our data sources.
  // This shouldn't happen except in unit tests.
  if (profile_) {
    // Check to see if the history service is ready. Because it loads async, it
    // may not be ready when the Bridge is created. If this happens, register
    // for a notification that tells us the HistoryService is ready.
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs != NULL && hs->backend_loaded()) {
      history_service_ = hs;
      Init();
    }

    // TODO(???): NULL here means we're OTR. Show this in the GUI somehow?
    tab_restore_service_ = profile_->GetTabRestoreService();
    if (tab_restore_service_)
      tab_restore_service_->AddObserver(this);
  }

  // The service is not ready for use yet, so become notified when it does.
  if (!history_service_) {
    registrar_.Add(this,
                   NotificationType::HISTORY_LOADED,
                   NotificationService::AllSources());
  }
}

HistoryMenuBridge::~HistoryMenuBridge() {
  // Unregister ourselves as observers and notifications.
  const NotificationSource& src = NotificationService::AllSources();
  if (history_service_) {
    registrar_.Remove(this, NotificationType::HISTORY_TYPED_URLS_MODIFIED, src);
    registrar_.Remove(this, NotificationType::HISTORY_URL_VISITED, src);
    registrar_.Remove(this, NotificationType::HISTORY_URLS_DELETED, src);
  } else {
    registrar_.Remove(this, NotificationType::HISTORY_LOADED, src);
  }

  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void HistoryMenuBridge::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  // A history service is now ready. Check to see if it's the one for the main
  // profile. If so, perform final initialization.
  if (type == NotificationType::HISTORY_LOADED) {
    HistoryService* hs =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs != NULL && hs->backend_loaded()) {
      history_service_ = hs;
      Init();

      // Found our HistoryService, so stop listening for this notification.
      registrar_.Remove(this,
                        NotificationType::HISTORY_LOADED,
                        NotificationService::AllSources());
    }
  }

  // All other notification types that we observe indicate that the history has
  // changed and we need to rebuild.
  need_recreate_ = true;
  CreateMenu();
}

void HistoryMenuBridge::TabRestoreServiceChanged(TabRestoreService* service) {
  const TabRestoreService::Entries& entries = service->entries();

  // Clear the history menu before modifying |closed_results_|.
  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, IDC_HISTORY_MENU_CLOSED, closed_results_.size());

  unsigned int added_count = 0;
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < kRecentlyClosedCount; ++it) {
    TabRestoreService::Entry* entry = *it;

    // If we have a window, loop over all of its tabs. This could consume all
    // of |kRecentlyClosedCount| in a given outer loop iteration.
    if (entry->type == TabRestoreService::WINDOW) {
      TabRestoreService::Window* entry_win = (TabRestoreService::Window*)entry;
      std::vector<TabRestoreService::Tab> tabs = entry_win->tabs;
      std::vector<TabRestoreService::Tab>::const_iterator it;
      for (it = tabs.begin(); it != tabs.end() &&
          added_count < kRecentlyClosedCount; ++it) {
        TabRestoreService::Tab tab = *it;
        if (AddNavigationForTab(tab))
          ++added_count;
      }
    } else if (entry->type == TabRestoreService::TAB) {
      TabRestoreService::Tab* tab =
          static_cast<TabRestoreService::Tab*>(entry);
      if (AddNavigationForTab(*tab))
        ++added_count;
    }
  }

  // Remove extraneous/old results.
  if (closed_results_.size() > kRecentlyClosedCount)
    closed_results_.erase(closed_results_.begin(),
                          closed_results_.end() - kRecentlyClosedCount);

  NSInteger top_index = [menu indexOfItemWithTag:IDC_HISTORY_MENU_CLOSED] + 1;

  int i = 0;  // Count offsets for |tag| and |index| in AddItemToMenu().
  for (std::vector<HistoryItem>::const_iterator it = closed_results_.begin();
      it != closed_results_.end(); ++it) {
    HistoryItem item = *it;
    NSInteger tag = IDC_HISTORY_MENU_CLOSED + 1 + i;
    AddItemToMenu(item, HistoryMenu(), tag, top_index + i);
    ++i;
  }
}

void HistoryMenuBridge::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  // Intentionally left blank. We hold a weak reference to the service.
}

HistoryService* HistoryMenuBridge::service() {
  return history_service_;
}

Profile* HistoryMenuBridge::profile() {
  return profile_;
}

std::vector<HistoryMenuBridge::HistoryItem>*
    HistoryMenuBridge::visited_results() {
  return &visited_results_;
}

std::vector<HistoryMenuBridge::HistoryItem>*
    HistoryMenuBridge::closed_results() {
  return &closed_results_;
}

NSMenu* HistoryMenuBridge::HistoryMenu() {
  NSMenu* history_menu = [[[NSApp mainMenu] itemWithTag:IDC_HISTORY_MENU]
                            submenu];
  return history_menu;
}

void HistoryMenuBridge::ClearMenuSection(NSMenu* menu,
                                         NSInteger tag,
                                         unsigned int count) {
  const NSInteger max_tag = tag + count + 1;

  // Get the index of the first item in the section, excluding the header.
  NSInteger index = [menu indexOfItemWithTag:tag] + 1;
  if (index <= 0 || index >= [menu numberOfItems])
    return;  // The section is at the end, empty.

  // Remove at the same index, usually, because the menu will shrink by one
  // item each time, shifting all the lower elements up. If we hit a "unhooked"
  // menu item, don't remove it, but advance the index to skip the item.
  NSInteger item_tag = tag;
  while (count > 0 && item_tag < max_tag && index < [menu numberOfItems]) {
    NSMenuItem* menu_item = [menu itemAtIndex:index];
    item_tag = [menu_item tag];
    if ([menu_item action] == @selector(openHistoryMenuItem:)) {
      [menu removeItemAtIndex:index];
      --count;
    }
    else {
      ++index;
    }
  }
}

void HistoryMenuBridge::AddItemToMenu(const HistoryItem& item,
                                      NSMenu* menu,
                                      NSInteger tag,
                                      NSInteger index) {
  NSString* title = base::SysUTF16ToNSString(item.title);
  std::string url_string = item.url.possibly_invalid_spec();

  // If we don't have a title, use the URL.
  if ([title isEqualToString:@""])
    title = base::SysUTF8ToNSString(url_string);
  NSString* full_title = title;
  if (false && [title length] > kMaximumMenuWidthInChars) {
    // TODO(rsesek): use app/gfx/text_elider.h once it uses string16 and can
    // take out the middle of strings.
    title = [NSString stringWithFormat:@"%@…%@",
               [title substringToIndex:kMenuTrimSizeInChars],
               [title substringFromIndex:([title length] -
                                          kMenuTrimSizeInChars)]];
  }
  scoped_nsobject<NSMenuItem> menu_item(
      [[NSMenuItem alloc] initWithTitle:title
                                 action:nil
                          keyEquivalent:@""]);
  [menu_item setTarget:controller_];
  [menu_item setAction:@selector(openHistoryMenuItem:)];
  [menu_item setTag:tag];

  // Add a tooltip.
  NSString* tooltip = [NSString stringWithFormat:@"%@\n%s", full_title,
                                url_string.c_str()];
  [menu_item setToolTip:tooltip];

  [menu insertItem:menu_item atIndex:index];
}

void HistoryMenuBridge::Init() {
  const NotificationSource& source = NotificationService::AllSources();
  registrar_.Add(this, NotificationType::HISTORY_TYPED_URLS_MODIFIED, source);
  registrar_.Add(this, NotificationType::HISTORY_URL_VISITED, source);
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED, source);
}

void HistoryMenuBridge::CreateMenu() {
  // If we're currently running CreateMenu(), wait until it finishes.
  if (create_in_progress_)
    return;
  create_in_progress_ = true;
  need_recreate_ = false;

  history_service_->QuerySegmentUsageSince(
      &cancelable_request_consumer_,
      base::Time::Now() - base::TimeDelta::FromDays(kMostVisitedScope),
      kMostVisitedCount,
      NewCallback(this, &HistoryMenuBridge::OnVisitedHistoryResults));
}

void HistoryMenuBridge::OnVisitedHistoryResults(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* results) {
  NSMenu* menu = HistoryMenu();
  NSInteger top_item = [menu indexOfItemWithTag:IDC_HISTORY_MENU_VISITED] + 1;

  ClearMenuSection(menu, IDC_HISTORY_MENU_VISITED, visited_results_.size());
  visited_results_.clear();

  size_t count = results->size();
  for (size_t i = 0; i < count; ++i) {
    PageUsageData* history_item = (*results)[i];

    HistoryItem item;
    item.title = history_item->GetTitle();
    item.url = history_item->GetURL();
    visited_results_.push_back(item);

    // Use the large gaps in tags assignment to create the tag for history menu
    // items.
    NSInteger tag = IDC_HISTORY_MENU_VISITED + 1 + i;
    AddItemToMenu(item, HistoryMenu(), tag, top_item + i);
  }

  // We are already invalid by the time we finished, darn.
  if (need_recreate_)
    CreateMenu();

  create_in_progress_ = false;
}

bool HistoryMenuBridge::AddNavigationForTab(
    const TabRestoreService::Tab& entry) {
  if (entry.navigations.empty())
    return false;

  const TabNavigation& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  if (current_navigation.url() == GURL(chrome::kChromeUINewTabURL))
    return false;

  HistoryItem item;
  item.title = current_navigation.title();
  item.url = current_navigation.url();
  closed_results_.push_back(item);
  return true;
}
