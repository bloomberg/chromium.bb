// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/new_task_manager_view.h"
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
    const GURL& url,
    const security_state::SecurityStateModel::SecurityInfo& security_info) {
  WebsiteSettingsPopupView::ShowPopup(
      nullptr, gfx::Rect(anchor_point, gfx::Size()), profile, web_contents, url,
      security_info);
}

void ShowBookmarkBubbleViewsAtPoint(const gfx::Point& anchor_point,
                                    gfx::NativeView parent,
                                    bookmarks::BookmarkBubbleObserver* observer,
                                    Browser* browser,
                                    const GURL& url,
                                    bool already_bookmarked) {
  // The Views dialog may prompt for sign in.
  std::unique_ptr<BubbleSyncPromoDelegate> delegate(
      new BookmarkBubbleSignInDelegate(browser));

  BookmarkBubbleView::ShowBubble(nullptr, gfx::Rect(anchor_point, gfx::Size()),
                                 parent, observer, std::move(delegate),
                                 browser->profile(), url, already_bookmarked);
}

ui::TableModel* ShowTaskManagerViews(Browser* browser) {
  // On platforms other than Mac, the new task manager is shown unless
  // explicitly disabled. Assume that running with ToolkitViewsDialogsEnabled()
  // on Mac also means the new task manager is desired.
  return task_management::NewTaskManagerView::Show(browser);
}

void HideTaskManagerViews() {
  task_management::NewTaskManagerView::Hide();
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
