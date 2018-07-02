// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

#include "components/signin/core/browser/signin_metrics.h"

@class MDCFlatButton;
@protocol SigninPromoViewDelegate;

typedef NS_ENUM(NSInteger, SigninPromoViewUI) {
  // The legacy SigninPromoViewUI style.
  SigninPromoViewUILegacy,
  // The SigninPromoViewRefreshUI style. In order for this value to take effect
  // the UIRefreshPhase1 flag also needs to be enabled. Otherwise
  // SigninPromoViewUILegacy style will still be used.
  SigninPromoViewUIRefresh,
};

typedef NS_ENUM(NSInteger, SigninPromoViewMode) {
  // No identity available on the device.
  SigninPromoViewModeColdState,
  // At least one identity is available on the device and the user can sign
  // without entering their credentials.
  SigninPromoViewModeWarmState,
};

extern NSString* const kSigninPromoViewId;
extern NSString* const kSigninPromoPrimaryButtonId;
extern NSString* const kSigninPromoSecondaryButtonId;
extern NSString* const kSigninPromoCloseButtonId;

// This class creates an image view, a label and 2 buttons. This view can be
// configured with 2 modes : "Cold State" and "Warm State".
// + "Cold State" mode displays the chomium icon in the image view, and only
//   displays the primary button.
// + "Warm State" mode displays the image view (big than the cold state mode),
//   displays both buttons.
//
//  For the warm state, the owner should set:
//   - the image for |imageView|, using -[SigninPromoView setProfileImage:]
//   - the label for |textLabel|
//   - the title for |primaryButton|
//   - the title for |secondaryButton|
@interface SigninPromoView : UIView

@property(nonatomic, weak) id<SigninPromoViewDelegate> delegate;
@property(nonatomic) SigninPromoViewMode mode;
@property(nonatomic, readonly) UIImageView* imageView;
@property(nonatomic, readonly) UILabel* textLabel;
@property(nonatomic, readonly) UIButton* primaryButton;
@property(nonatomic, readonly) UIButton* secondaryButton;
// Hidden by default.
@property(nonatomic, readonly) UIButton* closeButton;

// Horizontal padding used for |textLabel|, |primaryButton| and
// |secondaryButton|. Used to compute the preferred max layout width of
// |textLabel|.
@property(nonatomic, readonly) CGFloat horizontalPadding;

// Designated initializer. |signinPromoViewUI| sets the style for the
// SigninPromoView to legacy or refreshed. This same view is used on Collections
// and Settings, while the Collections UI will be refreshed soon, we still need
// to support the legacy UI for settings.
- (instancetype)initWithFrame:(CGRect)frame
                        style:(SigninPromoViewUI)signinPromoViewUI
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the image in |imageView|. This method will add a circular background
// using CircularImageFromImage() (so if the image is not squared, it will be
// cropped first). Must only be called in the "Warm State" mode.
- (void)setProfileImage:(UIImage*)image;

// Resets the view to be reused.
- (void)prepareForReuse;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_H_
