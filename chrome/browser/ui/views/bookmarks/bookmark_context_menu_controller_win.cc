// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "win8/util/win8_util.h"

using content::OpenURLParams;
using content::PageNavigator;
using content::UserMetricsAction;
using content::WebContents;

namespace {

// A PageNavigator implementation that creates a new Browser. This is used when
// opening a url and there is no Browser open. The Browser is created the first
// time the PageNavigator method is invoked.
class NewBrowserPageNavigator : public PageNavigator {
 public:
  explicit NewBrowserPageNavigator(Profile* profile)
      : profile_(profile),
        browser_(NULL) {}

  virtual ~NewBrowserPageNavigator() {
    if (browser_)
      browser_->window()->Show();
  }

  Browser* browser() const { return browser_; }

  virtual WebContents* OpenURL(const OpenURLParams& params) OVERRIDE {
    if (!browser_) {
      Profile* profile = (params.disposition == OFF_THE_RECORD) ?
          profile_->GetOffTheRecordProfile() : profile_;
      browser_ = new Browser(Browser::CreateParams(profile,
                                                   chrome::GetActiveDesktop()));
    }

    OpenURLParams forward_params = params;
    forward_params.disposition = NEW_FOREGROUND_TAB;
    return browser_->OpenURL(forward_params);
  }

 private:
  Profile* profile_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserPageNavigator);
};

}  // namespace

bool BookmarkContextMenuController::IsPlatformCommandIdEnabled(
    int command_id,
    bool* enabled) const {
  // In Windows 8 metro mode no new window option on a regular chrome window
  // and no new incognito window option on an incognito chrome window.
  if (win8::IsSingleWindowMetroMode()) {
    if (command_id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW &&
        !profile_->IsOffTheRecord()) {
      *enabled = false;
      return true;
    } else if (command_id == IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO &&
               profile_->IsOffTheRecord()) {
      *enabled = false;
      return true;
    }
  }
  return false;
}

bool BookmarkContextMenuController::ExecutePlatformCommand(int command_id,
                                                           int event_flags) {
  if (win8::IsSingleWindowMetroMode()) {
    switch (command_id) {
      // We need to handle the open in new window and open in incognito window
      // commands to ensure that they first look for an existing browser object
      // to handle the request. If we find one then a new foreground tab is
      // opened, else a new browser object is created.
      case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO: {
        Profile* profile_to_use = profile_;
        if (command_id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
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

        NewBrowserPageNavigator navigator_impl(profile_to_use);

        // TODO(robertshield): FTB - Switch this to HOST_DESKTOP_TYPE_ASH when
        //                     we make that the default for metro.
        Browser* browser =
            chrome::FindTabbedBrowser(profile_to_use,
                                      false,
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
        PageNavigator* navigator = NULL;
        if (!browser || !browser->tab_strip_model()->GetActiveWebContents()) {
          navigator = &navigator_impl;
        } else {
          browser->window()->Activate();
          navigator = browser->tab_strip_model()->GetActiveWebContents();
        }

        chrome::OpenAll(parent_window_, navigator, selection_,
                        NEW_FOREGROUND_TAB, profile_to_use);
        RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_CONTEXT_MENU);
        return true;
      }

      default:
        break;
    }
  }

  return false;
}
