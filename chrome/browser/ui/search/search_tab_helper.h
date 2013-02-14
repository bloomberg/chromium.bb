// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/search_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace chrome {
namespace search {

// Per-tab search "helper".  Acts as the owner and controller of the tab's
// search UI model.
class SearchTabHelper : public content::NotificationObserver,
                        public content::WebContentsUserData<SearchTabHelper> {
 public:
  virtual ~SearchTabHelper();

  SearchModel* model() {
    return &model_;
  }

  // Invoked when the OmniboxEditModel changes state in some way that might
  // affect the search mode.
  void OmniboxEditModelChanged(bool user_input_in_progress, bool cancelling);

  // Invoked when the active navigation entry is updated in some way that might
  // affect the search mode. This is used by Instant when it "fixes up" the
  // virtual URL of the active entry. Regular navigations are captured through
  // the notification system and shouldn't call this method.
  void NavigationEntryUpdated();

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the mode of the model based on the current URL of web_contents().
  void UpdateModel();

  // Returns the web contents associated with the tab that owns this helper.
  const content::WebContents* web_contents() const;

  const bool is_search_enabled_;

  // Tracks the last value passed to OmniboxEditModelChanged().
  bool user_input_in_progress_;

  // Model object for UI that cares about search state.
  SearchModel model_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
