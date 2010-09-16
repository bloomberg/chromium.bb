// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"

// Incognito profiles are not long-lived, so we always want to store a
// non-incognito profile.
//
// TODO(akalin): Should we make it so that we have a default incognito
// profile that's long-lived?  Of course, we'd still have to clear it out
// when all incognito browsers close.
HtmlDialogTabContentsDelegate::HtmlDialogTabContentsDelegate(Profile* profile)
    : profile_(profile->GetOriginalProfile()) {}

HtmlDialogTabContentsDelegate::~HtmlDialogTabContentsDelegate() {}

Profile* HtmlDialogTabContentsDelegate::profile() const { return profile_; }

void HtmlDialogTabContentsDelegate::Detach() {
  profile_ = NULL;
}

Browser* HtmlDialogTabContentsDelegate::CreateBrowser() {
  DCHECK(profile_);
  return Browser::Create(profile_);
}

void HtmlDialogTabContentsDelegate::OpenURLFromTab(
    TabContents* source, const GURL& url, const GURL& referrer,
    WindowOpenDisposition disposition, PageTransition::Type transition) {
  if (profile_) {
    // Force all links to open in a new window, ignoring the incoming
    // disposition. This is a tabless, modal dialog so we can't just
    // open it in the current frame.  Code adapted from
    // Browser::OpenURLFromTab() with disposition == NEW_WINDOW.
    Browser* browser = CreateBrowser();
    TabContents* new_contents =
        browser->AddTabWithURL(url, referrer, transition, -1,
                               TabStripModel::ADD_SELECTED, NULL,
                               std::string(), &browser);
    DCHECK(new_contents);
    browser->window()->Show();
    new_contents->Focus();
  }
}

void HtmlDialogTabContentsDelegate::NavigationStateChanged(
    const TabContents* source, unsigned changed_flags) {
  // We shouldn't receive any NavigationStateChanged except the first
  // one, which we ignore because we're a dialog box.
}

void HtmlDialogTabContentsDelegate::AddNewContents(
    TabContents* source, TabContents* new_contents,
    WindowOpenDisposition disposition, const gfx::Rect& initial_pos,
    bool user_gesture) {
  if (profile_) {
    // Force this to open in a new window, too.  Code adapted from
    // Browser::AddNewContents() with disposition == NEW_WINDOW.
    Browser* browser = CreateBrowser();
    static_cast<TabContentsDelegate*>(browser)->
        AddNewContents(source, new_contents, NEW_FOREGROUND_TAB,
                       initial_pos, user_gesture);
    browser->window()->Show();
  }
}

void HtmlDialogTabContentsDelegate::ActivateContents(TabContents* contents) {
  // We don't do anything here because there's only one TabContents in
  // this frame and we don't have a TabStripModel.
}

void HtmlDialogTabContentsDelegate::DeactivateContents(TabContents* contents) {
  // We don't care about this notification (called when a user gesture triggers
  // a call to window.blur()).
}

void HtmlDialogTabContentsDelegate::LoadingStateChanged(TabContents* source) {
  // We don't care about this notification.
}

void HtmlDialogTabContentsDelegate::CloseContents(TabContents* source) {
  // We receive this message but don't handle it because we really do the
  // cleanup somewhere else (namely, HtmlDialogUIDelegate::OnDialogClosed()).
}

bool HtmlDialogTabContentsDelegate::IsPopup(const TabContents* source) const {
  // This needs to return true so that we are allowed to be resized by our
  // contents.
  return true;
}

void HtmlDialogTabContentsDelegate::URLStarredChanged(TabContents* source,
                                                      bool starred) {
  // We don't have a visible star to click in the window.
  NOTREACHED();
}

void HtmlDialogTabContentsDelegate::UpdateTargetURL(TabContents* source,
                                                    const GURL& url) {
  // Ignored.
}

bool HtmlDialogTabContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType::Type navigation_type) {
  return false;
}

