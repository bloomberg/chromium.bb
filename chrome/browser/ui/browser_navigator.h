// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#pragma once

#include <string>

#include "content/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class Profile;
class TabContentsWrapper;

namespace browser {

// Parameters that tell Navigate() what to do.
//
// Some basic examples:
//
// Simple Navigate to URL in current tab:
// browser::NavigateParams params(browser, GURL("http://www.google.com/"),
//                                PageTransition::LINK);
// browser::Navigate(&params);
//
// Open bookmark in new background tab:
// browser::NavigateParams params(browser, url, PageTransition::AUTO_BOOKMARK);
// params.disposition = NEW_BACKGROUND_TAB;
// browser::Navigate(&params);
//
// Opens a popup TabContents:
// browser::NavigateParams params(browser, popup_contents);
// params.source_contents = source_contents;
// browser::Navigate(&params);
//
// See browser_navigator_browsertest.cc for more examples.
//
struct NavigateParams {
  NavigateParams(Browser* browser,
                 const GURL& a_url,
                 PageTransition::Type a_transition);
  NavigateParams(Browser* browser, TabContentsWrapper* a_target_contents);
  ~NavigateParams();

  // The URL/referrer to be loaded. Ignored if |target_contents| is non-NULL.
  GURL url;
  GURL referrer;

  // [in]  A TabContents to be navigated or inserted into the target Browser's
  //       tabstrip. If NULL, |url| or the homepage will be used instead. When
  //       non-NULL, Navigate() assumes it has already been navigated to its
  //       intended destination and will not load any URL in it (i.e. |url| is
  //       ignored).
  //       Default is NULL.
  // [out] The TabContents in which the navigation occurred or that was
  //       inserted. Guaranteed non-NULL except for note below:
  // Note: If this field is set to NULL by the caller and Navigate() creates
  //       a new TabContents, this field will remain NULL and the TabContents
  //       deleted if the TabContents it created is not added to a TabStripModel
  //       before Navigate() returns.
  TabContentsWrapper* target_contents;

  // [in]  The TabContents that initiated the Navigate() request if such context
  //       is necessary. Default is NULL, i.e. no context.
  // [out] If NULL, this value will be set to the selected TabContents in the
  //       originating browser prior to the operation performed by Navigate().
  //       However, if the originating page is from a different profile (e.g. an
  //       OFF_THE_RECORD page originating from a non-OTR window), then
  //       |source_contents| is reset to NULL.
  TabContentsWrapper* source_contents;

  // The disposition requested by the navigation source. Default is
  // CURRENT_TAB. What follows is a set of coercions that happen to this value
  // when other factors are at play:
  //
  // [in]:                Condition:                        [out]:
  // NEW_BACKGROUND_TAB   target browser tabstrip is empty  NEW_FOREGROUND_TAB
  // CURRENT_TAB          "     "     "                     NEW_FOREGROUND_TAB
  // OFF_THE_RECORD       target browser profile is incog.  NEW_FOREGROUND_TAB
  //
  // If disposition is NEW_BACKGROUND_TAB, TabStripModel::ADD_ACTIVE is
  // removed from |tabstrip_add_types| automatically.
  // If disposition is one of NEW_WINDOW, NEW_POPUP, NEW_FOREGROUND_TAB or
  // SINGLETON_TAB, then TabStripModel::ADD_ACTIVE is automatically added to
  // |tabstrip_add_types|.
  WindowOpenDisposition disposition;

  // The transition type of the navigation. Default is PageTransition::LINK
  // when target_contents is specified in the constructor.
  PageTransition::Type transition;

  // The index the caller would like the tab to be positioned at in the
  // TabStrip. The actual index will be determined by the TabHandler in
  // accordance with |add_types|. Defaults to -1 (allows the TabHandler to
  // decide).
  int tabstrip_index;

  // A bitmask of values defined in TabStripModel::AddTabTypes. Helps
  // determine where to insert a new tab and whether or not it should be
  // selected, among other properties. Default is ADD_ACTIVE.
  int tabstrip_add_types;

  // If non-empty, the new tab is an app tab.
  std::string extension_app_id;

  // If non-empty, specifies the desired initial position and size of the
  // window if |disposition| == NEW_POPUP.
  // TODO(beng): Figure out if this can be used to create Browser windows
  //             for other callsites that use set_override_bounds, or
  //             remove this comment.
  gfx::Rect window_bounds;

  // Determines if and how the target window should be made visible at the end
  // of the call to Navigate().
  enum WindowAction {
    // Do not show or activate the browser window after navigating.
    NO_ACTION,
    // Show and activate the browser window after navigating.
    SHOW_WINDOW,
    // Show the browser window after navigating but do not activate.
    SHOW_WINDOW_INACTIVE
  };
  // Default is NO_ACTION (don't show or activate the window).
  // If disposition is NEW_WINDOW or NEW_POPUP, and |window_action| is set to
  // NO_ACTION, |window_action| will be set to SHOW_WINDOW.
  WindowAction window_action;

  // What to do with the path component of the URL for singleton navigations.
  enum PathBehavior {
    // Two URLs with differing paths are different.
    RESPECT,
    // Ignore path when finding existing tab, navigate to new URL.
    IGNORE_AND_NAVIGATE,
    // Ignore path when finding existing tab, don't navigate tab.
    IGNORE_AND_STAY_PUT,
  };
  // Default is RESPECT.
  PathBehavior path_behavior;

  // [in]  Specifies a Browser object where the navigation could occur or the
  //       tab could be added. Navigate() is not obliged to use this Browser if
  //       it is not compatible with the operation being performed. If NULL,
  //       |profile| should be specified to find or create a matching Browser.
  // [out] Specifies the Browser object where the navigation occurred or the
  //       tab was added. Guaranteed non-NULL unless the disposition did not
  //       require a navigation, in which case this is set to NULL
  //       (SUPPRESS_OPEN, SAVE_TO_DISK, IGNORE_ACTION).
  // Note: If |show_window| is set to false and a new Browser is created by
  //       Navigate(), the caller is responsible for showing it so that its
  //       window can assume responsibility for the Browser's lifetime (Browser
  //       objects are deleted when the user closes a visible browser window).
  Browser* browser;

  // If |browser| == NULL, specifies a Profile to use when finding or
  // creating a Browser.
  Profile* profile;

 private:
  NavigateParams();
};

// Navigates according to the configuration specified in |params|.
void Navigate(NavigateParams* params);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
