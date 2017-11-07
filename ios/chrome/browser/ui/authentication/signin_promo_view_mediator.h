// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_delegate.h"

@class ChromeIdentity;
@protocol SigninPresenter;
@class SigninPromoViewConfigurator;
@protocol SigninPromoViewConsumer;

namespace ios {
class ChromeBrowserState;

// Enums for the sign-in promo view state.
enum class SigninPromoViewState {
  // None of the buttons has been used yet.
  Unused = 0,
  // Sign-in is in progress.
  SigninStarted,
  // Sign-in buttons has been used at least once.
  UsedAtLeastOnce,
  // Sign-in promo has been closed.
  Closed,
  // Sign-in promo view has been removed.
  Invalid,
};
}  // namespace ios

// Class that monitors the available identities and creates
// SigninPromoViewConfigurator. This class makes the link between the model and
// the view. The consumer will receive notification if default identity is
// changed or updated.
@interface SigninPromoViewMediator : NSObject<SigninPromoViewDelegate>

// Consumer to handle identity update notifications.
@property(nonatomic, weak) id<SigninPromoViewConsumer> consumer;

// Chrome identity used to configure the view in a warm state mode. Otherwise
// contains nil.
@property(nonatomic, readonly, strong) ChromeIdentity* defaultIdentity;

// Sign-in promo view state.
@property(nonatomic) ios::SigninPromoViewState signinPromoViewState;

// See -[SigninPromoViewMediator initWithBrowserState:].
- (instancetype)init NS_UNAVAILABLE;

// Initialises with browser state.
// * |browserState| is the current browser state.
// * |accessPoint| only ACCESS_POINT_SETTINGS, ACCESS_POINT_BOOKMARK_MANAGER,
// ACCESS_POINT_RECENT_TABS, ACCESS_POINT_TAB_SWITCHER are supported.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                           presenter:(id<SigninPresenter>)presenter
    NS_DESIGNATED_INITIALIZER;

- (SigninPromoViewConfigurator*)createConfigurator;

// Increments the "shown" counter used for histograms. Called when the signin
// promo view is visible
- (void)signinPromoViewVisible;

// Called when the sign-in promo view is hidden.
- (void)signinPromoViewHidden;

// Called when the sign-in promo view is closed.
- (void)signinPromoViewClosed;

// Called when the sign-in promo view is removed from the view hierarchy (it or
// one of its superviews is removed). The mediator should not be used after this
// called.
- (void)signinPromoViewRemoved;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
