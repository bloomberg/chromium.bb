// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabstrip.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

int GetIndexOfTab(const Browser* browser,
                  const content::WebContents* contents) {
  return browser->tab_strip_model()->GetIndexOfWebContents(contents);
}

TabContents* GetActiveTabContents(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveTabContents();
}

content::WebContents* GetActiveWebContents(const Browser* browser) {
  TabContents* active_tab = GetActiveTabContents(browser);
  return active_tab ? active_tab->web_contents() : NULL;
}

TabContents* GetTabContentsAt(const Browser* browser, int index) {
  return browser->tab_strip_model()->GetTabContentsAt(index);
}

content::WebContents* GetWebContentsAt(const Browser* browser, int index) {
  TabContents* tab = GetTabContentsAt(browser, index);
  return tab ? tab->web_contents() : NULL;
}

void ActivateTabAt(Browser* browser, int index, bool user_gesture) {
  browser->tab_strip_model()->ActivateTabAt(index, user_gesture);
}

bool IsTabStripEditable(Browser* browser) {
  return browser->window()->IsTabStripEditable();
}

TabContents* AddSelectedTabWithURL(Browser* browser,
                                   const GURL& url,
                                   content::PageTransition transition) {
  browser::NavigateParams params(browser, url, transition);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  return params.target_contents;
}

void AddTab(Browser* browser,
            TabContents* tab_contents,
            content::PageTransition type) {
  browser->tab_strip_model()->AddTabContents(tab_contents, -1, type,
                                             TabStripModel::ADD_ACTIVE);
}

void AddWebContents(Browser* browser,
                    content::WebContents* source_contents,
                    content::WebContents* new_contents,
                    WindowOpenDisposition disposition,
                    const gfx::Rect& initial_pos,
                    bool user_gesture) {
  // No code for this yet.
  DCHECK(disposition != SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK(disposition != CURRENT_TAB);

  TabContents* source_tab_contents = NULL;
  BlockedContentTabHelper* source_blocked_content = NULL;
  TabContents* new_tab_contents = TabContents::FromWebContents(new_contents);
  if (!new_tab_contents)
    new_tab_contents = new TabContents(new_contents);
  if (source_contents) {
    source_tab_contents = TabContents::FromWebContents(source_contents);
    source_blocked_content = source_tab_contents->blocked_content_tab_helper();
  }

  if (source_tab_contents) {
    // Handle blocking of all contents.
    if (source_blocked_content->all_contents_blocked()) {
      source_blocked_content->AddTabContents(new_tab_contents,
                                             disposition,
                                             initial_pos,
                                             user_gesture);
      return;
    }

    // Handle blocking of popups.
    if ((disposition == NEW_POPUP) && !user_gesture &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisablePopupBlocking)) {
      // Unrequested popups from normal pages are constrained unless they're in
      // the white list.  The popup owner will handle checking this.
      source_tab_contents->blocked_content_tab_helper()->
          AddPopup(new_tab_contents, initial_pos, user_gesture);
      return;
    }

    new_contents->GetRenderViewHost()->DisassociateFromPopupCount();
  }

  browser::NavigateParams params(browser, new_tab_contents);
  params.source_contents = source_contents ?
      GetTabContentsAt(browser, GetIndexOfTab(browser, source_contents)) : NULL;
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  browser::Navigate(&params);
}

void CloseWebContents(Browser* browser, content::WebContents* contents) {
  int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED() << "CloseWebContents called for tab not in our strip";
    return;
  }
  browser->tab_strip_model()->CloseTabContentsAt(
      index,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void CloseAllTabs(Browser* browser) {
  browser->tab_strip_model()->CloseAllTabs();
}

TabContents* TabContentsFactory(
    Profile* profile,
    content::SiteInstance* site_instance,
    int routing_id,
    const content::WebContents* base_web_contents,
    content::SessionStorageNamespace* session_storage_namespace) {
  return new TabContents(content::WebContents::Create(
      profile,
      site_instance,
      routing_id,
      base_web_contents,
      session_storage_namespace));
}

}  // namespace chrome
