// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"

#import <QuartzCore/QuartzCore.h>
#include <cmath>

#include "base/logging.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_button.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/gfx/scoped_ui_graphics_push_context_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kBarHeight = 48.0f;

const CGFloat kRegularLayoutButtonWidth = 168;

const int kOverlayViewColor = 0x5A7EF5;
const int kOverlayColorWidth = 98;
const int kNumberOfTabsIncognito = 2;

}  // anonymous namespace

@interface NewTabPageBar () {
  UIImageView* shadow_;
}

@property(nonatomic, readwrite, strong) NSArray* buttons;
@property(nonatomic, readwrite, strong) UIButton* popupButton;

- (void)setup;
- (void)calculateButtonWidth;
- (void)setupButton:(UIButton*)button;
- (BOOL)useIconsInButtons;
- (BOOL)showOverlay;
@end

@implementation NewTabPageBar {
  // Which button is currently selected.
  NSUInteger selectedIndex_;
  // Don't allow tabbar animations on startup, only after first tap.
  BOOL canAnimate_;
  __weak id<NewTabPageBarDelegate> delegate_;
  // Logo view, used to center the tab buttons.
  UIImageView* logoView_;
  // Overlay view, used to highlight the selected button.
  UIImageView* overlayView_;
  // Overlay view, used to highlight the selected button.
  UIView* overlayColorView_;
  // Width of a button.
  CGFloat buttonWidth_;
  // Percentage overlay sits over tab bar buttons.
  CGFloat overlayPercentage_;
}

@synthesize items = items_;
@synthesize selectedIndex = selectedIndex_;
@synthesize popupButton = popupButton_;
@synthesize buttons = buttons_;
@synthesize delegate = delegate_;
@synthesize overlayPercentage = overlayPercentage_;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setup];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self) {
    [self setup];
  }
  return self;
}

- (void)setup {
  self.selectedIndex = NSNotFound;
  canAnimate_ = NO;
  self.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;
  self.autoresizesSubviews = YES;
  self.backgroundColor = [UIColor clearColor];

  if ([self showOverlay]) {
    overlayView_ =
        [[UIImageView alloc] initWithFrame:CGRectMake(0, 0, buttonWidth_, 2)];

    // Center |overlayColorView_| inside |overlayView_|.
    CGFloat colorX = AlignValueToPixel((buttonWidth_ - kOverlayColorWidth) / 2);
    overlayColorView_ = [[UIView alloc]
        initWithFrame:CGRectMake(colorX, 0, kOverlayColorWidth, 2)];
    [overlayColorView_
        setBackgroundColor:UIColorFromRGB(kOverlayViewColor, 1.0)];
    [overlayColorView_ layer].cornerRadius = 1.0;
    [overlayColorView_
        setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin |
                            UIViewAutoresizingFlexibleRightMargin];
    [overlayView_ addSubview:overlayColorView_];
    [self addSubview:overlayView_];
  }

  // Make the drop shadow.
  UIImage* shadowImage = [UIImage imageNamed:@"ntp_bottom_bar_shadow"];
  shadow_ = [[UIImageView alloc] initWithImage:shadowImage];
  // Shadow is positioned directly above the new tab page bar.
  [shadow_
      setFrame:CGRectMake(0, -shadowImage.size.height, self.bounds.size.width,
                          shadowImage.size.height)];
  [shadow_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [self addSubview:shadow_];

  self.contentMode = UIViewContentModeRedraw;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // |buttonWidth_| changes with the screen orientation when the NTP button bar
  // is enabled.
  [self calculateButtonWidth];

  CGFloat logoWidth = logoView_.image.size.width;
  CGFloat padding = [self useIconsInButtons] ? logoWidth : 0;
  CGFloat buttonPadding = floor((CGRectGetWidth(self.bounds) - padding -
                                 buttonWidth_ * self.buttons.count) /
                                    2 +
                                padding);

  for (NSUInteger i = 0; i < self.buttons.count; ++i) {
    NewTabPageBarButton* button = [self.buttons objectAtIndex:i];
    LayoutRect layout = LayoutRectMake(
        buttonPadding + (i * buttonWidth_), CGRectGetWidth(self.bounds), 0,
        buttonWidth_, CGRectGetHeight(self.bounds));
    button.frame = LayoutRectGetRect(layout);
    [button
        setContentToDisplay:[self useIconsInButtons]
                                ? new_tab_page_bar_button::ContentType::IMAGE
                                : new_tab_page_bar_button::ContentType::TEXT];
  }

  // Position overlay image over percentage of tab bar button(s).
  CGRect frame = [overlayView_ frame];
  frame.origin.x = floor(
      buttonWidth_ * self.buttons.count * overlayPercentage_ + buttonPadding);
  frame.size.width = buttonWidth_;
  DCHECK(!std::isnan(frame.origin.x));
  DCHECK(!std::isnan(frame.size.width));
  [overlayView_ setFrame:frame];
}

- (CGSize)sizeThatFits:(CGSize)size {
  return CGSizeMake(size.width, kBarHeight);
}

- (void)calculateButtonWidth {
  if (IsCompact()) {
    if ([items_ count] > 0) {
      buttonWidth_ = self.bounds.size.width / [items_ count];
    } else {
      // In incognito on phones, there are no items shown.
      buttonWidth_ = 0;
    }
    return;
  }

  buttonWidth_ = kRegularLayoutButtonWidth;
}

// When setting a new set of items on the tab bar, the buttons need to be
// regenerated and the old buttons need to be removed.
- (void)setItems:(NSArray*)newItems {
  if (newItems == items_)
    return;

  items_ = newItems;
  // Remove all the existing buttons from the view.
  for (UIButton* button in self.buttons) {
    [button removeFromSuperview];
  }

  // Create a set of new buttons.
  [self calculateButtonWidth];
  if (newItems.count) {
    NSMutableArray* newButtons = [NSMutableArray array];
    for (NSUInteger i = 0; i < newItems.count; ++i) {
      NewTabPageBarItem* item = [newItems objectAtIndex:i];
      NewTabPageBarButton* button = [NewTabPageBarButton buttonWithItem:item];
      button.frame = CGRectIntegral(CGRectMake(
          i * buttonWidth_, 0, buttonWidth_, self.bounds.size.height));
      [self setupButton:button];
      [self addSubview:button];
      [newButtons addObject:button];
    }
    self.buttons = newButtons;
  } else {
    self.buttons = nil;
  }
  [self setNeedsLayout];
}

- (void)setOverlayPercentage:(CGFloat)overlayPercentage {
  DCHECK(!std::isnan(overlayPercentage));
  overlayPercentage_ = overlayPercentage;
  [self setNeedsLayout];
}

- (void)setupButton:(UIButton*)button {
  // Old NTP tab bar buttons have a non-standard control event for firing
  // actions.  Because they are tied to a scrollView that reacts immediately,
  // and on phone there is a tooltip that fires on touchdown that needs to
  // display while on the new tab, the control event for -buttonDidTap is touch
  // down. On phone with dialogs enabled, treat the NTP buttons as normal
  // buttons, where the standard is to fire an action on touch up inside.
  if ([self showOverlay]) {
    [button addTarget:self
                  action:@selector(buttonDidTap:)
        forControlEvents:UIControlEventTouchDown];
  } else {
    [button addTarget:self
                  action:@selector(buttonDidTap:)
        forControlEvents:UIControlEventTouchUpInside];
  }
}

- (void)setSelectedIndex:(NSUInteger)newIndex {
  // When not showing the overlay, the Bookmarks and Recent Tabs are displayed
  // in modal view controllers, so the tab bar buttons should not be selected.
  if (![self showOverlay])
    return;
  if (newIndex != self.selectedIndex) {
    if (newIndex < self.items.count) {
      for (NSUInteger i = 0; i < self.buttons.count; ++i) {
        UIButton* button = [self.buttons objectAtIndex:i];
        button.selected = (i == newIndex);
      }
    }
    selectedIndex_ = newIndex;
  }
}

- (void)buttonDidTap:(UIButton*)button {
  canAnimate_ = YES;
  NSUInteger buttonIndex = [self.buttons indexOfObject:button];
  if (buttonIndex != NSNotFound) {
    self.selectedIndex = buttonIndex;
    [delegate_ newTabBarItemDidChange:[self.items objectAtIndex:buttonIndex]
                          changePanel:YES];
  }
}

- (void)setShadowAlpha:(CGFloat)alpha {
  CGFloat currentAlpha = [shadow_ alpha];
  CGFloat diff = std::abs(alpha - currentAlpha);
  if (diff >= 1) {
    [UIView animateWithDuration:0.3
                     animations:^{
                       [shadow_ setAlpha:alpha];
                     }];
  } else {
    [shadow_ setAlpha:alpha];
  }
}

- (void)updateColorsForScrollView:(UIScrollView*)scrollView {
  if (![self showOverlay]) {
    return;
  }

  UIColor* backgroundViewColorBookmarks = [UIColor whiteColor];
  CGFloat viewWidth = self.bounds.size.width;
  CGFloat progress = LeadingContentOffsetForScrollView(scrollView) / viewWidth;
  progress = fminf(progress, 1.0f);
  progress = fmaxf(progress, 0.0f);

  if ([self.items count] == kNumberOfTabsIncognito) {
    UIColor* overlayViewColor = UIColorFromRGB(kOverlayViewColor, 1.0);
    UIColor* overlayViewColorIncognito = [UIColor whiteColor];
    UIColor* updatedOverlayColor = InterpolateFromColorToColor(
        overlayViewColor, overlayViewColorIncognito, progress);
    [overlayColorView_ setBackgroundColor:updatedOverlayColor];

    UIColor* backgroundViewColorIncognito =
        [UIColor colorWithWhite:34 / 255.0 alpha:1.0];
    UIColor* backgroundColor = InterpolateFromColorToColor(
        backgroundViewColorBookmarks, backgroundViewColorIncognito, progress);
    self.backgroundColor = backgroundColor;
    scrollView.backgroundColor = backgroundColor;

    for (NewTabPageBarButton* button in self.buttons) {
      [button useIncognitoColorScheme:progress];
    }
  } else {
    UIColor* backgroundColor = backgroundViewColorBookmarks;
    self.backgroundColor = backgroundViewColorBookmarks;
    scrollView.backgroundColor = backgroundColor;
  }
}

- (BOOL)useIconsInButtons {
  return PresentNTPPanelModally() || IsCompactTablet();
}

- (BOOL)showOverlay {
  // The bar buttons launch modal dialogs on tap on iPhone. Don't show overlay
  // in this case.
  return !PresentNTPPanelModally();
}

@end
