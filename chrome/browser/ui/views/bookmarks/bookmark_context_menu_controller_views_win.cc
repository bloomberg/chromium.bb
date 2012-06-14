// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu_controller_views_win.h"

#include "base/win/metro.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;

// static
BookmarkContextMenuControllerViews* BookmarkContextMenuControllerViews::Create(
      views::Widget* parent_widget,
      BookmarkContextMenuControllerViewsDelegate* delegate,
      Profile* profile,
      content::PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection) {
  return new BookmarkContextMenuControllerViewsWin(parent_widget, delegate,
                                                   profile, navigator, parent,
                                                   selection);
}

BookmarkContextMenuControllerViewsWin::BookmarkContextMenuControllerViewsWin(
      views::Widget* parent_widget,
      BookmarkContextMenuControllerViewsDelegate* delegate,
      Profile* profile,
      content::PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection)
    : BookmarkContextMenuControllerViews(parent_widget, delegate, profile,
                                         navigator, parent, selection) {
}

BookmarkContextMenuControllerViewsWin
    ::~BookmarkContextMenuControllerViewsWin() {
}

void BookmarkContextMenuControllerViewsWin::ExecuteCommand(int id) {
  if (base::win::GetMetroModule()) {
    switch (id) {
      // We need to handle the open in new window and open in incognito window
      // commands to ensure that they first look for an existing browser object
      // to handle the request. If we find one then a new foreground tab is
      // opened, else a new browser object is created.
      case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO: {
        Profile* profile_to_use = profile();
        if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
          if (profile_to_use->IsOffTheRecord())
            profile_to_use = profile_to_use->GetOriginalProfile();

          content::RecordAction(
              UserMetricsAction("BookmarkBar_ContextMenu_OpenAllInNewWindow"));
        } else {
          if (!profile_to_use->IsOffTheRecord())
            profile_to_use = profile_to_use->GetOffTheRecordProfile();

          content::RecordAction(
              UserMetricsAction("BookmarkBar_ContextMenu_OpenAllIncognito"));
        }
        // Passing in NULL for the PageNavigator ensures that we first look for
        // an existing browser window to handle the request before trying to
        // create one.
        bookmark_utils::OpenAll(parent_widget()->GetNativeWindow(),
                                profile_to_use, NULL, selection(),
                                NEW_FOREGROUND_TAB);
        bookmark_utils::RecordBookmarkLaunch(
            bookmark_utils::LAUNCH_CONTEXT_MENU);
        return;
      }

      default:
        break;
    }
  }
  BookmarkContextMenuControllerViews::ExecuteCommand(id);
}

bool BookmarkContextMenuControllerViewsWin::IsCommandEnabled(int id) const {
  // In Windows 8 metro mode no new window option on a regular chrome window
  // and no new incognito window option on an incognito chrome window.
  if (base::win::GetMetroModule()) {
    if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW &&
        !profile()->IsOffTheRecord()) {
      return false;
    } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO &&
               profile()->IsOffTheRecord()) {
      return false;
    }
  }
  return BookmarkContextMenuControllerViews::IsCommandEnabled(id);
}
