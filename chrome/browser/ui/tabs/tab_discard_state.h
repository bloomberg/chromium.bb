// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DISCARD_STATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_DISCARD_STATE_H_

#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

// Manages the information about the discarding state of a tab. This data is
// stored in WebContents.
class TabDiscardState : public base::SupportsUserData::Data {
 public:
  // Returns the TabDiscardState associated with |web_contents|. It will create
  // one if none exists.
  static TabDiscardState* Get(content::WebContents* web_contents);

  // Sets the TabDiscardState of a |web_contents|. The object referenced by
  // |state| is now owned by |web_contents|
  static void Set(content::WebContents* web_contents, TabDiscardState* state);

  // Returns true if |web_contents| has been discarded to save memory.
  static bool IsDiscarded(content::WebContents* web_contents);

  // Sets/clears the discard state of |web_contents|.
  static void SetDiscardState(content::WebContents* web_contents, bool state);

  // Returns the number of times |web_contents| has been discarded.
  static int DiscardCount(content::WebContents* web_contents);

  // Increments the number of times |web_contents| has been discarded.
  static void IncrementDiscardCount(content::WebContents* web_contents);

  // Copies the discard state from |old_contents| to |new_contents|.
  static void CopyState(content::WebContents* old_contents,
                        content::WebContents* new_contents);

  TabDiscardState() : is_discarded_(false), discard_count_(0) {}

 private:
  // Is the tab currently discarded?
  bool is_discarded_;

  // Number of times the tab has been discarded.
  int discard_count_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DISCARD_STATE_H_
