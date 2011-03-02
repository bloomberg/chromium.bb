// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace printing {

PrintPreviewTabController::PrintPreviewTabController()
    : waiting_for_new_preview_page_(false) {
}

PrintPreviewTabController::~PrintPreviewTabController() {}

// static
PrintPreviewTabController* PrintPreviewTabController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_tab_controller();
}

TabContents* PrintPreviewTabController::GetOrCreatePreviewTab(
    TabContents* initiator_tab, int browser_window_id ) {
  DCHECK(initiator_tab);

  // Get the print preview tab for |initiator_tab|.
  TabContents* preview_tab = GetPrintPreviewForTab(initiator_tab);
  if (preview_tab) {
    // Show current preview tab.
    preview_tab->Activate();
    return preview_tab;
  }
  return CreatePrintPreviewTab(initiator_tab, browser_window_id);
}

TabContents* PrintPreviewTabController::GetPrintPreviewForTab(
    TabContents* tab) const {
  PrintPreviewTabMap::const_iterator it = preview_tab_map_.find(tab);
  if (it != preview_tab_map_.end())
    return tab;

  for (it = preview_tab_map_.begin(); it != preview_tab_map_.end(); ++it) {
    // If |tab| is an initiator tab.
    if (tab == it->second) {
      // Return the associated preview tab.
      return it->first;
    }
  }
  return NULL;
}

void PrintPreviewTabController::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  TabContents* initiator_tab = NULL;
  TabContents* preview_tab = NULL;
  TabContents* source_tab = NULL;
  NavigationController::LoadCommittedDetails* detail_info = NULL;

  switch (type.value) {
    case NotificationType::TAB_CONTENTS_DESTROYED: {
      source_tab = Source<TabContents>(source).ptr();
      break;
    }
    case NotificationType::NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      source_tab = controller->tab_contents();
      detail_info =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      break;
    }
    default: {
      NOTREACHED();
      return;
    }
  }

  DCHECK(source_tab);
  preview_tab = GetPrintPreviewForTab(source_tab);

  // |source_tab| is preview tab.
  if (preview_tab == source_tab)
    initiator_tab = GetInitiatorTab(source_tab);
  else
    initiator_tab = source_tab;

  if (detail_info) {
    PageTransition::Type transition_type =
        detail_info->entry->transition_type();
    NavigationType::Type nav_type = detail_info->type;

    // Don't update/erase the map entry if the page has not changed.
    if (transition_type == PageTransition::RELOAD ||
        nav_type == NavigationType::SAME_PAGE) {
      return;
    }

    // New |preview_tab| is created. Don't update/erase map entry.
    if (waiting_for_new_preview_page_ &&
        transition_type == PageTransition::LINK &&
        nav_type == NavigationType::NEW_PAGE &&
        source_tab == preview_tab) {
      waiting_for_new_preview_page_ = false;
      return;
    }

    // User navigated to a preview tab using forward/back button.
    if (IsPrintPreviewTab(source_tab) &&
        transition_type == PageTransition::FORWARD_BACK &&
        nav_type == NavigationType::EXISTING_PAGE) {
      return;
    }
  }

  // If |source_tab| is |initiator_tab|, update the map entry.
  if (source_tab == initiator_tab) {
    preview_tab_map_[preview_tab] = NULL;
  }

  // If |source_tab| is |preview_tab|, erase the map entry.
  if (source_tab == preview_tab) {
    preview_tab_map_.erase(preview_tab);
    RemoveObservers(preview_tab);
  }

  if (initiator_tab)
    RemoveObservers(initiator_tab);
}

// static
bool PrintPreviewTabController::IsPrintPreviewTab(TabContents* tab) {
  const GURL& url = tab->GetURL();
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

TabContents* PrintPreviewTabController::GetInitiatorTab(
    TabContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

TabContents* PrintPreviewTabController::CreatePrintPreviewTab(
    TabContents* initiator_tab, int browser_window_id) {
  Browser* current_browser = BrowserList::FindBrowserWithID(browser_window_id);
  // Add a new tab next to initiator tab.
  browser::NavigateParams params(current_browser,
                                 GURL(chrome::kChromeUIPrintURL),
                                 PageTransition::LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = current_browser->tabstrip_model()->
      GetWrapperIndex(initiator_tab) + 1;
  browser::Navigate(&params);
  TabContentsWrapper* preview_tab = params.target_contents;
  preview_tab->tab_contents()->Activate();

  // Add an entry to the map.
  preview_tab_map_[preview_tab->tab_contents()] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab->tab_contents());

  return preview_tab->tab_contents();
}

void PrintPreviewTabController::AddObservers(TabContents* tab) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab));
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&tab->controller()));
}

void PrintPreviewTabController::RemoveObservers(TabContents* tab) {
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(tab));
  registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
                    Source<NavigationController>(&tab->controller()));
}

}  // namespace printing
