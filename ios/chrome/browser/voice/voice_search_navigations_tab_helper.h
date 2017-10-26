// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_

#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

namespace web {
class NavigationItem;
}

// A helper object that tracks which NavigationItems were created because of
// voice search queries.
class VoiceSearchNavigationTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<VoiceSearchNavigationTabHelper> {
 public:
  // Notifies that the next navigation is the result of a voice
  // search.
  void WillLoadVoiceSearchResult();

  // Returns whether |item| was created for a voice search query.
  bool IsNavigationFromVoiceSearch(const web::NavigationItem* item) const;

 private:
  friend class web::WebStateUserData<VoiceSearchNavigationTabHelper>;

  // Private constructor.
  explicit VoiceSearchNavigationTabHelper(web::WebState* web_state);

  // WebStateObserver:
  void NavigationItemCommitted(
      web::WebState* web_state,
      const web::LoadCommittedDetails& load_details) override;

  // Whether a voice search navigation is expected.
  bool will_navigate_to_voice_search_result_ = false;

  DISALLOW_COPY_AND_ASSIGN(VoiceSearchNavigationTabHelper);
};

#endif  // IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_
