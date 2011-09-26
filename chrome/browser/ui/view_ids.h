// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of a container view.

#ifndef CHROME_BROWSER_UI_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEW_IDS_H_
#pragma once

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
  VIEW_ID_LOCATION_BAR,
  VIEW_ID_APP_MENU,
  VIEW_ID_AUTOCOMPLETE,
  VIEW_ID_BROWSER_ACTION_TOOLBAR,
  VIEW_ID_FEEDBACK_BUTTON,

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
  VIEW_ID_TAB_CONTAINER_FOCUS_VIEW,

  // Docked dev tools.
  VIEW_ID_DEV_TOOLS_DOCKED,

  // The contents split.
  VIEW_ID_CONTENTS_SPLIT,

  // The Infobar container.
  VIEW_ID_INFO_BAR_CONTAINER,

  // The Download shelf.
  VIEW_ID_DOWNLOAD_SHELF,

  // The Sidebar container.
  VIEW_ID_SIDE_BAR_CONTAINER,

  // The sidebar split.
  VIEW_ID_SIDE_BAR_SPLIT,

  // Used in chrome/browser/ui/gtk/view_id_util_browsertests.cc
  // If you add new ids, make sure the above test passes.
  VIEW_ID_PREDEFINED_COUNT
};

#endif  // CHROME_BROWSER_UI_VIEW_IDS_H_

