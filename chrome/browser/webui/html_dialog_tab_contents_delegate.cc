// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/html_dialog_tab_contents_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

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

void HtmlDialogTabContentsDelegate::OpenURLFromTab(
    TabContents* source, const GURL& url, const GURL& referrer,
    WindowOpenDisposition disposition, PageTransition::Type transition) {
  if (profile_) {
    // Specify a NULL browser for navigation. This will cause Navigate()
    // to find a browser matching params.profile or create a new one.
    Browser* browser = NULL;
    browser::NavigateParams params(browser, url, transition);
    params.profile = profile_;
    params.referrer = referrer;
    params.disposition = disposition;
    params.show_window = true;
    browser::Navigate(&params);
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
    // Specify a NULL browser for navigation. This will cause Navigate()
    // to find a browser matching params.profile or create a new one.
    Browser* browser = NULL;

    TabContentsWrapper* wrapper = new TabContentsWrapper(new_contents);
    browser::NavigateParams params(browser, wrapper);
    params.profile = profile_;
    // TODO(pinkerton): no way to get a wrapper for this.
    // params.source_contents = source;
    params.disposition = disposition;
    params.window_bounds = initial_pos;
    params.show_window = true;
    browser::Navigate(&params);
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

void HtmlDialogTabContentsDelegate::UpdateTargetURL(TabContents* source,
                                                    const GURL& url) {
  // Ignored.
}

bool HtmlDialogTabContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType::Type navigation_type) {
  return false;
}
