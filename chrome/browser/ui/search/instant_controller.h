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
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/common/search/search_types.h"

class BrowserInstantController;

namespace content {
class WebContents;
}

// InstantController is responsible for updating the theme and most visited info
// of the current tab when
// * the user switches to an existing NTP tab, or
// * an open tab navigates to an NTP.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public InstantTab::Delegate {
 public:
  explicit InstantController(BrowserInstantController* browser);
  ~InstantController() override;

  // The search mode in the active tab has changed. Bind |instant_tab_| if the
  // |new_mode| reflects an Instant NTP.
  void SearchModeChanged(const SearchMode& old_mode,
                         const SearchMode& new_mode);

  // The user switched tabs. Bind |instant_tab_| if the newly active tab is an
  // Instant NTP.
  void ActiveTabChanged();

  // Resets list of debug events.
  void ClearDebugEvents();

  // See comments for |debug_events_| below.
  const std::list<std::pair<int64_t, std::string>>& debug_events() {
    return debug_events_;
  }

 private:
  friend class InstantExtendedManualTest;
  friend class InstantTestBase;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           SearchDoesntReuseInstantTabWithoutSupport);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           TypedSearchURLDoesntReuseInstantTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           DispatchMVChangeEventWhileNavigatingBackToNTP);

  // Overridden from InstantTab::Delegate:
  // TODO(shishir): We assume that the WebContent's current RenderViewHost is
  // the RenderViewHost being created which is not always true. Fix this.
  void InstantTabAboutToNavigateMainFrame(const content::WebContents* contents,
                                          const GURL& url) override;

  // Adds a new event to |debug_events_| and also DVLOG's it. Ensures that
  // |debug_events_| doesn't get too large.
  void LogDebugEvent(const std::string& info) const;

  // If the active tab is an Instant NTP, sets |instant_tab_| to point to it.
  // Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Sends theme info and most visited items to the Instant renderer process.
  void UpdateInfoForInstantTab();

  BrowserInstantController* const browser_;

  // The instance of InstantTab maintained by InstantController.
  std::unique_ptr<InstantTab> instant_tab_;

  // The search model mode for the active tab.
  SearchMode search_mode_;

  // List of events and their timestamps, useful in debugging Instant behaviour.
  mutable std::list<std::pair<int64_t, std::string>> debug_events_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
