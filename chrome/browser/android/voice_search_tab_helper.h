// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VOICE_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_VOICE_SEARCH_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Tab helper to toggle media autoplay for voice URL searches.
class VoiceSearchTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VoiceSearchTabHelper> {
 public:
  ~VoiceSearchTabHelper() override;

  // content::WebContentsObserver overrides.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

 private:
  explicit VoiceSearchTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<VoiceSearchTabHelper>;

  bool gesture_requirement_for_playback_disabled_;

  DISALLOW_COPY_AND_ASSIGN(VoiceSearchTabHelper);
};

#endif  // CHROME_BROWSER_ANDROID_VOICE_SEARCH_TAB_HELPER_H_
