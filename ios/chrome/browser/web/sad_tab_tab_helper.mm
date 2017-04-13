// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(SadTabTabHelper);

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state,
                                 id<SadTabTabHelperDelegate> delegate)
    : web::WebStateObserver(web_state), delegate_(delegate) {
  DCHECK(delegate_);
}

SadTabTabHelper::~SadTabTabHelper() = default;

void SadTabTabHelper::CreateForWebState(web::WebState* web_state,
                                        id<SadTabTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           new SadTabTabHelper(web_state, delegate));
  }
}

void SadTabTabHelper::RenderProcessGone() {
  if (!delegate_ || [delegate_ isTabVisibleForTabHelper:this]) {
    PresentSadTab();
  }
}

void SadTabTabHelper::PresentSadTab() {
  SadTabView* sad_tab_view = [[SadTabView alloc] initWithReloadHandler:^{
    web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL, true);
  }];

  CRWContentView* content_view =
      [[CRWGenericContentView alloc] initWithView:sad_tab_view];

  web_state()->ShowTransientContentView(content_view);
}
