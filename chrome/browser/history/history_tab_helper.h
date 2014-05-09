// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class HistoryService;

namespace history {
struct HistoryAddPageArgs;
}

class HistoryTabHelper : public content::WebContentsObserver,
                         public content::WebContentsUserData<HistoryTabHelper> {
 public:
  virtual ~HistoryTabHelper();

  // Updates history with the specified navigation. This is called by
  // OnMsgNavigate to update history state.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // Sends the page title to the history service. This is called when we receive
  // the page title and we know we want to update history.
  void UpdateHistoryPageTitle(const content::NavigationEntry& entry);

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history.
  history::HistoryAddPageArgs CreateHistoryAddPageArgs(
      const GURL& virtual_url,
      base::Time timestamp,
      bool did_replace_entry,
      const content::FrameNavigateParams& params);

 private:
  explicit HistoryTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryTabHelper>;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // Helper function to return the history service.  May return NULL.
  HistoryService* GetHistoryService();

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history. This
  // prevents some weirdness because some AJAXy apps use titles for status
  // messages.
  bool received_page_title_;

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelper);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
