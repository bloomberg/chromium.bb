// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"

class BrowserInstantController;

namespace content {
class WebContents;
}  // namespace content

// InstantController is responsible for updating the theme and most visited info
// of the current tab when
// * the user switches to an existing NTP tab, or
// * an open tab navigates to an NTP.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController {
 public:
  explicit InstantController(
      BrowserInstantController* browser_instant_controller);
  ~InstantController();

  void OnTabActivated(content::WebContents* web_contents);
  void OnTabDeactivated(content::WebContents* web_contents);
  void OnTabDetached(content::WebContents* web_contents);

  // Resets list of debug events.
  void ClearDebugEvents();

  // See comments for |debug_events_| below.
  const std::list<std::pair<int64_t, std::string>>& debug_events() {
    return debug_events_;
  }

 private:
  class TabObserver;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           SearchDoesntReuseInstantTabWithoutSupport);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           TypedSearchURLDoesntReuseInstantTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           DispatchMVChangeEventWhileNavigatingBackToNTP);

  // Adds a new event to |debug_events_| and also DVLOG's it. Ensures that
  // |debug_events_| doesn't get too large.
  void LogDebugEvent(const std::string& info) const;

  // Sends theme info and most visited items to the Instant renderer process.
  void UpdateInfoForInstantTab();

  BrowserInstantController* const browser_instant_controller_;

  // Observes the currently active tab, and calls us back if it becomes an NTP.
  std::unique_ptr<TabObserver> tab_observer_;

  // List of events and their timestamps, useful in debugging Instant behaviour.
  mutable std::list<std::pair<int64_t, std::string>> debug_events_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
