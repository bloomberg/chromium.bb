// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_LEGACY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_TABS_LEGACY_TAB_HELPER_H_

#import "base/ios/weak_nsobject.h"
#include "base/macros.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class Tab;

// LegacyTabHelper allows to access to the Tab owning a given WebState for
// interoperability of code using WebStates with legacy code using Tabs.
class LegacyTabHelper : public web::WebStateUserData<LegacyTabHelper> {
 public:
  // Creates the LegacyTabHelper to record the association of |web_state|
  // with |tab|. It is an error if |web_state| is already associated with
  // another Tab.
  static void CreateForWebState(web::WebState* web_state, Tab* tab);

  // Returns the Tab associated with |web_state| if it exists or nil.
  static Tab* GetTabForWebState(web::WebState* web_state);

 private:
  LegacyTabHelper(web::WebState* web_state, Tab* tab);
  ~LegacyTabHelper() override;

  // The Tab instance associated with the WebState. The Tab currently owns
  // the WebState, so this should only be nil between the call to -[Tab close]
  // and the object is deallocated.
  base::WeakNSObject<Tab> tab_;

  DISALLOW_COPY_AND_ASSIGN(LegacyTabHelper);
};

#endif  // IOS_CHROME_BROWSER_TABS_LEGACY_TAB_HELPER_H_
