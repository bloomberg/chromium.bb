// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/legacy_tab_helper.h"

#import "ios/chrome/browser/tabs/tab.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(LegacyTabHelper);

// static
void LegacyTabHelper::CreateForWebState(web::WebState* web_state, Tab* tab) {
  DCHECK(web_state);
  DCHECK(!FromWebState(web_state));
  web_state->SetUserData(UserDataKey(), new LegacyTabHelper(web_state, tab));
}

// static
Tab* LegacyTabHelper::GetTabForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  LegacyTabHelper* tab_helper = LegacyTabHelper::FromWebState(web_state);
  return tab_helper ? tab_helper->tab_.get() : nil;
}

LegacyTabHelper::~LegacyTabHelper() = default;

LegacyTabHelper::LegacyTabHelper(web::WebState* web_state, Tab* tab)
    : tab_(tab) {
  DCHECK_EQ(web_state, tab.webState);
}
