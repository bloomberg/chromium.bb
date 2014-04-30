// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NAVIGATION_HANDLER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Records a UMA stat ("NewTabPage.ActionAndroid") for the action the user takes
// to navigate away from the NTP.
class NavigationHandler : public content::WebUIMessageHandler {
 public:
  NavigationHandler();
  virtual ~NavigationHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "openedMostVisited".
  void HandleOpenedMostVisited(const base::ListValue* args);

  // Callback for "openedRecentlyClosed".
  void HandleOpenedRecentlyClosed(const base::ListValue* args);

  // Callback for "openedBookmark".
  void HandleOpenedBookmark(const base::ListValue* args);

  // Callback for "openedForeignSession".
  void HandleOpenedForeignSession(const base::ListValue* args);

  static void RecordActionForNavigation(const content::NavigationEntry& entry);

 private:
  // Possible actions taken by the user on the NTP. This enum is also defined in
  // histograms.xml. WARNING: these values must stay in sync with histograms.xml
  // and new actions can be added only at the end of the enum.
  enum Action {
    // User performed a search using the omnibox
    ACTION_SEARCHED_USING_OMNIBOX = 0,
    // User navigated to Google search homepage using the omnibox
    ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE = 1,
    // User navigated to any other page using the omnibox
    ACTION_NAVIGATED_USING_OMNIBOX = 2,
    // User opened a most visited page
    ACTION_OPENED_MOST_VISITED_ENTRY = 3,
    // User opened a recently closed tab
    ACTION_OPENED_RECENTLY_CLOSED_ENTRY = 4,
    // User opened a bookmark
    ACTION_OPENED_BOOKMARK = 5,
    // User opened a foreign session (from other devices section)
    ACTION_OPENED_FOREIGN_SESSION = 6,
    // The number of possible actions
    NUM_ACTIONS = 7
  };

  static void RecordAction(Action action);

  DISALLOW_COPY_AND_ASSIGN(NavigationHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NAVIGATION_HANDLER_H_
