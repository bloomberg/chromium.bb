// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_message.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, public:

BrowserTabStripModelDelegate::BrowserTabStripModelDelegate(Browser* browser)
    : browser_(browser),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

BrowserTabStripModelDelegate::~BrowserTabStripModelDelegate() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, TabStripModelDelegate implementation:

TabContents* BrowserTabStripModelDelegate::AddBlankTab(bool foreground) {
  return chrome::AddBlankTab(browser_, foreground);
}

TabContents* BrowserTabStripModelDelegate::AddBlankTabAt(int index,
                                                         bool foreground) {
  return chrome::AddBlankTabAt(browser_, index, foreground);
}

Browser* BrowserTabStripModelDelegate::CreateNewStripWithContents(
    TabContents* detached_contents,
    const gfx::Rect& window_bounds,
    const DockInfo& dock_info,
    bool maximize) {
  DCHECK(browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP));

  gfx::Rect new_window_bounds = window_bounds;
  if (dock_info.GetNewWindowBounds(&new_window_bounds, &maximize))
    dock_info.AdjustOtherWindowBounds();

  // Create an empty new browser window the same size as the old one.
  Browser* browser = new Browser(Browser::TYPE_TABBED, browser_->profile());
  browser->set_override_bounds(new_window_bounds);
  browser->set_initial_show_state(
      maximize ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL);
  browser->InitBrowserWindow();
  browser->tab_strip_model()->AppendTabContents(detached_contents, true);
  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  // TODO(beng): find a better way of doing this.
  static_cast<content::WebContentsDelegate*>(browser)->
      LoadingStateChanged(detached_contents->web_contents());
  return browser;
}

int BrowserTabStripModelDelegate::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION |
      (browser_->tab_count() > 1 ? TabStripModelDelegate::TAB_MOVE_ACTION : 0);
}

TabContents* BrowserTabStripModelDelegate::CreateTabContentsForURL(
    const GURL& url, const content::Referrer& referrer, Profile* profile,
    content::PageTransition transition, bool defer_load,
    content::SiteInstance* instance) const {
  TabContents* contents = TabContentsFactory(profile, instance,
      MSG_ROUTING_NONE, GetActiveWebContents(browser_), NULL);
  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->web_contents()->GetController().LoadURL(
        url, referrer, transition, std::string());
  }

  return contents;
}

bool BrowserTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return CanDuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::DuplicateContentsAt(int index) {
  DuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::CloseFrameAfterDragSession() {
#if !defined(OS_MACOSX)
  // This is scheduled to run after we return to the message loop because
  // otherwise the frame will think the drag session is still active and ignore
  // the request.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserTabStripModelDelegate::CloseFrame,
                 weak_factory_.GetWeakPtr()));
#endif
}

void BrowserTabStripModelDelegate::CreateHistoricalTab(TabContents* contents) {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  if (!browser_->profile() || browser_->profile()->IsOffTheRecord())
    return;

  // We don't create historical tabs for print preview tabs.
  if (contents->web_contents()->GetURL() == GURL(kChromeUIPrintURL))
    return;

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    service->CreateHistoricalTab(contents->web_contents(),
        browser_->tab_strip_model()->GetIndexOfTabContents(contents));
  }
}

bool BrowserTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    TabContents* contents) {
  return Browser::RunUnloadEventsHelper(contents->web_contents());
}

bool BrowserTabStripModelDelegate::CanBookmarkAllTabs() const {
  return chrome::CanBookmarkAllTabs(browser_);
}

void BrowserTabStripModelDelegate::BookmarkAllTabs() {
  chrome::BookmarkAllTabs(browser_);
}

bool BrowserTabStripModelDelegate::CanRestoreTab() {
  return chrome::CanRestoreTab(browser_);
}

void BrowserTabStripModelDelegate::RestoreTab() {
  chrome::RestoreTab(browser_);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, private:

void BrowserTabStripModelDelegate::CloseFrame() {
  browser_->window()->Close();
}

}  // namespace chrome
