// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class InstantPageTest;

// Per-tab search "helper".  Acts as the owner and controller of the tab's
// search UI model.
//
// When the page is finished loading, SearchTabHelper determines the instant
// support for the page. When a navigation entry is committed (except for
// in-page navigations), SearchTabHelper resets the instant support state to
// INSTANT_SUPPORT_UNKNOWN and cause support to be determined again.
class SearchTabHelper : public content::NotificationObserver,
                        public content::WebContentsObserver,
                        public content::WebContentsUserData<SearchTabHelper> {
 public:
  virtual ~SearchTabHelper();

  SearchModel* model() {
    return &model_;
  }

  // Invoked when the OmniboxEditModel changes state in some way that might
  // affect the search mode.
  void OmniboxEditModelChanged(bool user_input_in_progress,
                               bool cancelling,
                               bool popup_is_open,
                               bool user_text_is_empty);

  // Invoked when the active navigation entry is updated in some way that might
  // affect the search mode. This is used by Instant when it "fixes up" the
  // virtual URL of the active entry. Regular navigations are captured through
  // the notification system and shouldn't call this method.
  void NavigationEntryUpdated();

  // Updates |last_known_most_visited_items_| with |items|.
  // Returns false if |items| matches the |last_known_most_visited_items_|.
  bool UpdateLastKnownMostVisitedItems(
      const std::vector<InstantMostVisitedItem>& items);

  // Invoked to update the instant support state.
  void InstantSupportChanged(bool supports_instant);

  // Returns true if the page supports instant. If the instant support state is
  // not determined or if the page does not support instant returns false.
  bool SupportsInstant() const;

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;
  friend class InstantPageTest;
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_Local);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_NonLocal);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           PageURLDoesntBelongToInstantRenderer);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest, PageSupportsInstant);

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from contents::WebContentsObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Sets the mode of the model based on the current URL of web_contents().
  void UpdateMode();

  // Tells the renderer to determine if the page supports the Instant API, which
  // results in a call to OnInstantSupportDetermined() when the reply
  // is received.
  void DetermineIfPageSupportsInstant();

  // Handler for when Instant support has been determined.
  void OnInstantSupportDetermined(int page_id, bool supports_instant);

  // Handlers for SearchBox API to show and hide top bars (bookmark and info
  // bars).
  void OnSearchBoxShowBars(int page_id);
  void OnSearchBoxHideBars(int page_id);

  const bool is_search_enabled_;

  // Tracks the last value passed to OmniboxEditModelChanged().
  bool user_input_in_progress_;
  bool popup_is_open_;
  bool user_text_is_empty_;

  // Model object for UI that cares about search state.
  SearchModel model_;

  content::NotificationRegistrar registrar_;

  content::WebContents* web_contents_;

  // Tracks the last set of most visited items sent to the InstantPage renderer.
  // Used to prevent sending duplicate IPC messages to the renderer.
  std::vector<InstantMostVisitedItem> last_known_most_visited_items_;

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
