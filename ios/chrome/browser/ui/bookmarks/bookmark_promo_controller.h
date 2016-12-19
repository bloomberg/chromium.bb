// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

@protocol BookmarkPromoControllerDelegate

// Controls the state of the promo.
- (void)promoStateChanged:(BOOL)promoEnabled;

@end

// This controller manages the display of the promo cell through its delegate
// and handles displaying the sign-in view controller.
@interface BookmarkPromoController : NSObject

@property(nonatomic, assign) id<BookmarkPromoControllerDelegate> delegate;

// Holds the current state of the promo. When the promo state change, it will
// call the promoStateChanged: selector on the delegate.
@property(nonatomic, assign) BOOL promoState;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate;

// Presents the sign-in UI.
- (void)showSignIn;

// Hides the promo cell. It won't be presented again on this profile.
- (void)hidePromoCell;

// Updates the promo state based on the sign-in state of the user.
- (void)updatePromoState;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_
