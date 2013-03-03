// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of a container view.

#ifndef CHROME_BROWSER_UI_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEW_IDS_H_

enum ViewID {
  VIEW_ID_NONE = 0,

  // BROWSER WINDOW VIEWS
  // ------------------------------------------------------

  // Tabs within a window/tab strip, counting from the left.
  VIEW_ID_TAB_0,
  VIEW_ID_TAB_1,
  VIEW_ID_TAB_2,
  VIEW_ID_TAB_3,
  VIEW_ID_TAB_4,
  VIEW_ID_TAB_5,
  VIEW_ID_TAB_6,
  VIEW_ID_TAB_7,
  VIEW_ID_TAB_8,
  VIEW_ID_TAB_9,
  VIEW_ID_TAB_LAST,

  // ID for any tab. Currently only used on views.
  VIEW_ID_TAB,

  VIEW_ID_TAB_STRIP,

  // Toolbar & toolbar elements.
  VIEW_ID_TOOLBAR = 1000,
  VIEW_ID_BACK_BUTTON,
  VIEW_ID_FORWARD_BUTTON,
  VIEW_ID_RELOAD_BUTTON,
  VIEW_ID_HOME_BUTTON,
  VIEW_ID_STAR_BUTTON,
  VIEW_ID_APP_MENU,
  VIEW_ID_BROWSER_ACTION_TOOLBAR,
  VIEW_ID_FEEDBACK_BUTTON,
  VIEW_ID_OMNIBOX,
  VIEW_ID_SCRIPT_BUBBLE,

  // The Bookmark Bar.
  VIEW_ID_BOOKMARK_BAR,
  VIEW_ID_OTHER_BOOKMARKS,
  // Used for bookmarks/folders on the bookmark bar.
  VIEW_ID_BOOKMARK_BAR_ELEMENT,

  // Find in page.
  VIEW_ID_FIND_IN_PAGE_TEXT_FIELD,
  VIEW_ID_FIND_IN_PAGE,

  // Tab Container window.
  VIEW_ID_TAB_CONTAINER,

  // Docked dev tools.
  VIEW_ID_DEV_TOOLS_DOCKED,

  // The contents split.
  VIEW_ID_CONTENTS_SPLIT,

  // The Infobar container.
  VIEW_ID_INFO_BAR_CONTAINER,

  // The Download shelf.
  VIEW_ID_DOWNLOAD_SHELF,

// TODO(dbeam): change the zoom decoration to an NSImageView on Mac so IDs work.
#if !defined(OS_MACOSX)
  // Zoom button in location bar.
  VIEW_ID_ZOOM_BUTTON,
#endif

  // Used in chrome/browser/ui/gtk/view_id_util_browsertests.cc
  // If you add new ids, make sure the above test passes.
  VIEW_ID_PREDEFINED_COUNT,

  // Plus button on location bar.
  VIEW_ID_ACTION_BOX_BUTTON,
};

#endif  // CHROME_BROWSER_UI_VIEW_IDS_H_

