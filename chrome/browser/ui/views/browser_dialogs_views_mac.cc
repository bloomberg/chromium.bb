// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

// This file provides definitions of desktop browser dialog-creation methods for
// Mac where a Cocoa browser is using Views dialogs. I.e. it is included in the
// Cocoa build and definitions under chrome/browser/ui/cocoa may select at
// runtime whether to show a Cocoa dialog, or the toolkit-views dialog defined
// here (declared in browser_dialogs.h).

namespace chrome {

void ShowWebsiteSettingsBubbleViewsAtPoint(
    const gfx::Point& anchor_point,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& virtual_url,
    const security_state::SecurityInfo& security_info) {
  // Don't show the bubble again if it's already showing. A second click on the
  // location icon in the omnibox will dismiss an open bubble. This behaviour is
  // consistent with the non-Mac views implementation.
  // Note that when the browser is toolkit-views, IsPopupShowing() is checked
  // earlier because the popup is shown on mouse release (but dismissed on
  // mouse pressed). A Cocoa browser does both on mouse pressed, so a check
  // when showing is sufficient.
  if (WebsiteSettingsPopupView::GetShownPopupType() !=
      WebsiteSettingsPopupView::POPUP_NONE) {
    return;
  }

  WebsiteSettingsPopupView::ShowPopup(
      nullptr, gfx::Rect(anchor_point, gfx::Size()), profile, web_contents,
      virtual_url, security_info);
}

void ShowBookmarkBubbleViewsAtPoint(const gfx::Point& anchor_point,
                                    gfx::NativeView parent,
                                    bookmarks::BookmarkBubbleObserver* observer,
                                    Browser* browser,
                                    const GURL& virtual_url,
                                    bool already_bookmarked) {
  // The Views dialog may prompt for sign in.
  std::unique_ptr<BubbleSyncPromoDelegate> delegate(
      new BookmarkBubbleSignInDelegate(browser));

  BookmarkBubbleView::ShowBubble(
      nullptr, gfx::Rect(anchor_point, gfx::Size()), parent, observer,
      std::move(delegate), browser->profile(), virtual_url, already_bookmarked);
}

task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManagerViews() {
  task_manager::TaskManagerView::Hide();
}

void ContentSettingBubbleViewsBridge::Show(gfx::NativeView parent_view,
                                           ContentSettingBubbleModel* model,
                                           content::WebContents* web_contents,
                                           const gfx::Point& anchor) {
  ContentSettingBubbleContents* contents =
      new ContentSettingBubbleContents(model, web_contents, nullptr,
          views::BubbleBorder::Arrow::TOP_RIGHT);
  contents->set_parent_window(parent_view);
  contents->SetAnchorRect(gfx::Rect(anchor, gfx::Size()));
  views::BubbleDialogDelegateView::CreateBubble(contents)->Show();
}

}  // namespace chrome
