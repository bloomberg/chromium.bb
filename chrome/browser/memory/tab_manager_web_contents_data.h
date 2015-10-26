// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_

#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/memory/tab_manager.h"

namespace content {
class WebContents;
}

namespace memory {

// Manages the information about the discarding state of a tab. This data is
// stored in WebContents.
class TabManager::WebContentsData : public base::SupportsUserData::Data {
 public:
  // Returns the WebContentsData associated with |web_contents|. It will create
  // one if none exists.
  static WebContentsData* Get(content::WebContents* web_contents);

  // Sets the WebContentsData of a |web_contents|. The object referenced by
  // |state| is now owned by |web_contents|
  static void Set(content::WebContents* web_contents, WebContentsData* state);

  // Copies the discard state from |old_contents| to |new_contents|.
  static void CopyState(content::WebContents* old_contents,
                        content::WebContents* new_contents);

  // Returns true if |web_contents| has been discarded to save memory.
  static bool IsDiscarded(content::WebContents* web_contents);

  // Sets/clears the discard state of |web_contents|.
  static void SetDiscardState(content::WebContents* web_contents, bool state);

  // Returns the number of times |web_contents| has been discarded.
  static int DiscardCount(content::WebContents* web_contents);

  // Increments the number of times |web_contents| has been discarded.
  static void IncrementDiscardCount(content::WebContents* web_contents);

  // Returns true if audio has recently been audible from the WebContents.
  static bool IsRecentlyAudible(content::WebContents* web_contents);

  // Set/clears the state of whether audio has recently been audible from the
  // WebContents.
  static void SetRecentlyAudible(content::WebContents* web_contents,
                                 bool state);

  // Returns the timestamp of the last time |web_contents| changed its audio
  // state.
  static base::TimeTicks LastAudioChangeTime(
      content::WebContents* web_contents);

  // Sets the timestamp of the last time |web_contents| changed its audio state.
  static void SetLastAudioChangeTime(content::WebContents* web_contents,
                                     base::TimeTicks timestamp);

 private:
  WebContentsData();

  // Is the tab currently discarded?
  bool is_discarded_;

  // Number of times the tab has been discarded.
  int discard_count_;

  // Is the tab playing audio?
  bool is_recently_audible_;

  // Last time the tab started or stopped playing audio (we record the
  // transition time).
  base::TimeTicks last_audio_change_time_;

  // The last time the tab was discarded.
  base::TimeTicks last_discard_time_;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
