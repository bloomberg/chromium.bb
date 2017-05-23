// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(SadTabTabHelper);

namespace {
// The default window of time a failure of the same URL needs to occur
// to be considered a repeat failure.
NSTimeInterval const kDefaultRepeatFailureInterval = 60.0f;
}

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state,
                                 id<SadTabTabHelperDelegate> delegate)
    : SadTabTabHelper(web_state, delegate, kDefaultRepeatFailureInterval) {}

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state,
                                 id<SadTabTabHelperDelegate> delegate,
                                 double repeat_failure_interval)
    : web::WebStateObserver(web_state),
      delegate_(delegate),
      repeat_failure_interval_(repeat_failure_interval) {
  DCHECK(delegate_);
}

SadTabTabHelper::~SadTabTabHelper() = default;

void SadTabTabHelper::CreateForWebState(web::WebState* web_state,
                                        id<SadTabTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(), base::WrapUnique(new SadTabTabHelper(
                                              web_state, delegate)));
  }
}

void SadTabTabHelper::CreateForWebState(web::WebState* web_state,
                                        id<SadTabTabHelperDelegate> delegate,
                                        double repeat_failure_interval) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new SadTabTabHelper(
                               web_state, delegate, repeat_failure_interval)));
  }
}

void SadTabTabHelper::RenderProcessGone() {
  if (!delegate_ || [delegate_ isTabVisibleForTabHelper:this]) {
    PresentSadTab(web_state()->GetLastCommittedURL());
  }
}

void SadTabTabHelper::DidFinishNavigation(
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetUrl().host() == kChromeUICrashHost &&
      navigation_context->GetUrl().scheme() == kChromeUIScheme) {
    PresentSadTab(navigation_context->GetUrl());
  }
}

void SadTabTabHelper::PresentSadTab(const GURL& url_causing_failure) {
  // Is this failure a repeat-failure requiring the presentation of the Feedback
  // UI rather than the Reload UI?
  double seconds_since_last_failure =
      last_failed_timer_ ? last_failed_timer_->Elapsed().InSecondsF() : DBL_MAX;

  bool repeated_failure =
      (url_causing_failure.EqualsIgnoringRef(last_failed_url_) &&
       seconds_since_last_failure < repeat_failure_interval_);

  SadTabView* sad_tab_view = [[SadTabView alloc]
           initWithMode:repeated_failure ? SadTabViewMode::FEEDBACK
                                         : SadTabViewMode::RELOAD
      navigationManager:web_state()->GetNavigationManager()];

  CRWContentView* content_view =
      [[CRWGenericContentView alloc] initWithView:sad_tab_view];

  web_state()->ShowTransientContentView(content_view);

  last_failed_url_ = url_causing_failure;
  last_failed_timer_ = base::MakeUnique<base::ElapsedTimer>();
}
