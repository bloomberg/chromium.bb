// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_STATS_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_STATS_H_

// This enum is also defined in histograms.xml. These values represent the
// action that the user has taken to leave the NTP. This is shared between the
// Suggested pane and the Most Visited pane. This is also defined in new_tab.js.
enum NtpFollowAction {
  // Values 0 to 10 are the core values from PageTransition (see
  // page_transition_types.h).

  // The user has clicked on a tile.
  NTP_FOLLOW_ACTION_CLICKED_TILE = 11,
  // The user has moved to another pane within the NTP.
  NTP_FOLLOW_ACTION_CLICKED_OTHER_NTP_PANE,
  // Any action other than what is listed here (e.g. closing the tab, etc.).
  NTP_FOLLOW_ACTION_OTHER,
  // The number of suggested sites actions that we log.
  NUM_NTP_FOLLOW_ACTIONS
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_STATS_H_
