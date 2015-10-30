// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/memory/tab_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace memory {

// Internal class used by TabManager to record the needed data for
// WebContentses.
class TabManager::WebContentsData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabManager::WebContentsData> {
 public:
  explicit WebContentsData(content::WebContents* web_contents);
  ~WebContentsData() override;

  // WebContentsObserver implementation:
  void DidStartLoading() override;
  void WebContentsDestroyed() override;

  // Returns true if |web_contents| has been discarded to save memory.
  bool IsDiscarded();

  // Sets/clears the discard state of |web_contents|.
  void SetDiscardState(bool state);

  // Returns the number of times |web_contents| has been discarded.
  int DiscardCount();

  // Increments the number of times |web_contents| has been discarded.
  void IncrementDiscardCount();

  // Returns true if audio has recently been audible from the WebContents.
  bool IsRecentlyAudible();

  // Set/clears the state of whether audio has recently been audible from the
  // WebContents.
  void SetRecentlyAudible(bool state);

  // Returns the timestamp of the last time |web_contents| changed its audio
  // state.
  base::TimeTicks LastAudioChangeTime();

  // Sets the timestamp of the last time |web_contents| changed its audio state.
  void SetLastAudioChangeTime(base::TimeTicks timestamp);

  // Copies the discard state from |old_contents| to |new_contents|.
  static void CopyState(content::WebContents* old_contents,
                        content::WebContents* new_contents);

 private:
  struct Data {
    Data();
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
    // The last time the tab was reloaded after being discarded.
    base::TimeTicks last_reload_time_;
  };

  Data tab_data_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
