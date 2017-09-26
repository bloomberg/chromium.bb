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
#include "chrome/browser/ui/search/search_model_observer.h"

class BrowserInstantController;

// InstantController is responsible for updating the theme and most visited info
// of the current tab when
// * the user switches to an existing NTP tab, or
// * an open tab navigates to an NTP.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public SearchModelObserver {
 public:
  explicit InstantController(
      BrowserInstantController* browser_instant_controller);
  ~InstantController() override;

  // SearchModelObserver:
  void ModelChanged(SearchModel::Origin old_origin,
                    SearchModel::Origin new_origin) override;

  // The user switched tabs. Bind |instant_tab_observer_| if the newly active
  // tab is an Instant NTP.
  void ActiveTabChanged();

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

  void InstantTabAboutToNavigateMainFrame();

  // Adds a new event to |debug_events_| and also DVLOG's it. Ensures that
  // |debug_events_| doesn't get too large.
  void LogDebugEvent(const std::string& info) const;

  // If the active tab is an Instant NTP, sets |instant_tab_| to point to it.
  // Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Sends theme info and most visited items to the Instant renderer process.
  void UpdateInfoForInstantTab();

  BrowserInstantController* const browser_instant_controller_;

  // Only non-null if the current tab is an Instant tab, i.e. an NTP.
  std::unique_ptr<TabObserver> instant_tab_observer_;

  // List of events and their timestamps, useful in debugging Instant behaviour.
  mutable std::list<std::pair<int64_t, std::string>> debug_events_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
