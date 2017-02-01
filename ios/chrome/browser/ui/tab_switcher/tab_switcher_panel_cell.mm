// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/fade_truncated_label.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_cache.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_utils.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#import "ios/third_party/material_text_accessibility_ios/src/src/MDFTextAccessibility.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkia;
}

namespace {
const CGFloat kFontSize = 16;
const CGFloat kCellCornerRadius = 2;
const CGFloat kBarHeight = 44;
const CGFloat kTitleLabelTextAlpha = .54;
const CGFloat kNewTabIconAlpha = .87;
}

CGFloat tabSwitcherLocalSessionCellTopBarHeight() {
  return kBarHeight;
}

@interface TabSwitcherSessionCell ()

// Returns the container view with rounded corners to which all cell subviews
// should be added.
- (UIView*)containerView;

@end

@implementation TabSwitcherSessionCell {
  base::scoped_nsobject<UIView> _containerView;
  CGSize _cachedShadowSize;
}

@synthesize delegate = _delegate;

// Returns the cell's identifier used for the cell's re-use.
+ (NSString*)identifier {
  return NSStringFromClass([self class]);
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self contentView].isAccessibilityElement = YES;
    _containerView.reset([[UIView alloc] initWithFrame:self.bounds]);
    [_containerView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                        UIViewAutoresizingFlexibleWidth];
    [[_containerView layer] setCornerRadius:kCellCornerRadius];
    [[_containerView layer] setMasksToBounds:YES];
    [[self contentView] addSubview:_containerView];
    [self updateShadow];
    [[[self contentView] layer] setShouldRasterize:YES];
    [[[self contentView] layer]
        setRasterizationScale:[[UIScreen mainScreen] scale]];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateShadow];
}

- (void)updateShadow {
  if (!CGSizeEqualToSize(_cachedShadowSize, self.bounds.size)) {
    CGRect offsetedRectangle = CGRectOffset(self.bounds, 0, 6);
    UIBezierPath* shadowPath =
        [UIBezierPath bezierPathWithRoundedRect:offsetedRectangle
                                   cornerRadius:kCellCornerRadius];
    [[self contentView].layer setShadowPath:shadowPath.CGPath];
    [[self contentView].layer setShadowColor:[UIColor blackColor].CGColor];
    [[self contentView].layer setShadowOpacity:0.5];
    _cachedShadowSize = self.bounds.size;
  }
}

- (UIView*)containerView {
  return _containerView.get();
}

@end

@implementation TabSwitcherLocalSessionCell {
  base::scoped_nsobject<UIView> _topBar;
  base::scoped_nsobject<UILabel> _titleLabel;
  base::scoped_nsobject<UIImageView> _favicon;
  base::scoped_nsobject<UIButton> _closeButton;
  base::scoped_nsobject<UIImageView> _shadow;
  base::scoped_nsobject<UIImageView> _snapshot;
  base::scoped_nsobject<TabSwitcherButton> _snapshotButton;
  PendingSnapshotRequest _currentPendingSnapshotRequest;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Top bar.
    _topBar.reset([[UIView alloc] initWithFrame:CGRectZero]);
    [_topBar setTranslatesAutoresizingMaskIntoConstraints:NO];
    [[self containerView] addSubview:_topBar];

    // Snapshot view.
    _snapshot.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [_snapshot setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_snapshot setContentMode:UIViewContentModeScaleAspectFill];
    [_snapshot setClipsToBounds:YES];
    [[self containerView] addSubview:_snapshot];

    // Cell button.
    _snapshotButton.reset([[TabSwitcherButton alloc] initWithFrame:CGRectZero]);
    [_snapshotButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_snapshotButton addTarget:self
                        action:@selector(snapshotPressed)
              forControlEvents:UIControlEventTouchUpInside];
    [[self containerView] addSubview:_snapshotButton];

    // Shadow view.
    _shadow.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [_shadow setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_shadow setImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW)];
    [[self containerView] addSubview:_shadow];

    // Constraints on the Top bar, snapshot view, and shadow view.
    NSDictionary* viewsDictionary = @{
      @"bar" : _topBar.get(),
      @"shadow" : _shadow.get(),
      @"snapshot" : _snapshot.get(),
      @"snapshotButton" : _snapshotButton.get(),
    };
    NSArray* constraints = @[
      @"H:|-0-[bar]-0-|",
      @"H:|-0-[shadow]-0-|",
      @"H:|-0-[snapshot]-0-|",
      @"H:|-0-[snapshotButton]-0-|",
      @"V:|-0-[bar(==barHeight)]-0-[snapshot]-0-|",
      @"V:[bar]-0-[snapshotButton]-0-|",
      @"V:[bar]-0-[shadow]",
    ];
    NSDictionary* metrics =
        @{ @"barHeight" : @(tabSwitcherLocalSessionCellTopBarHeight()) };
    ApplyVisualConstraintsWithMetrics(constraints, viewsDictionary, metrics,
                                      [self containerView]);

    // Create and add subviews to the cell bar.
    // Title label.
    _titleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_titleLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                             regularFontOfSize:kFontSize]];
    [_topBar addSubview:_titleLabel];

    // Favicon.
    _favicon.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [_favicon setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_topBar addSubview:_favicon];

    // Close button.
    _closeButton.reset([[UIButton alloc] initWithFrame:CGRectZero]);
    [_closeButton
        setImage:[[UIImage imageNamed:@"card_close_button"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    [_closeButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_closeButton addTarget:self
                     action:@selector(closeButtonPressed)
           forControlEvents:UIControlEventTouchUpInside];
    [_closeButton setExclusiveTouch:YES];
    [_topBar addSubview:_closeButton];

    // Constraints on the title label, favicon, and close button.
    NSDictionary* barViewsDictionary = @{
      @"favicon" : _favicon.get(),
      @"title" : _titleLabel.get(),
      @"closeButton" : _closeButton.get()
    };
    NSArray* barConstraints = @[
      @"H:|-16-[favicon(==24)]-8-[title]-0-[closeButton(==32)]-8-|",
      @"V:[favicon(==24)]",
      @"V:[closeButton(==32)]",
    ];
    ApplyVisualConstraints(barConstraints, barViewsDictionary, _topBar);
    AddSameCenterYConstraint(_topBar, _favicon);
    AddSameCenterYConstraint(_topBar, _titleLabel);
    AddSameCenterYConstraint(_topBar, _closeButton);
  }
  return self;
}

- (UIView*)topBar {
  return _topBar.get();
}

- (UIImage*)screenshot {
  return [_snapshot image];
}

- (void)setSnapshot:(UIImage*)image {
  DCHECK(!ImageHasAlphaChannel(image));
  [_snapshot setImage:image];
}

- (void)setAppearanceForTab:(Tab*)tab cellSize:(CGSize)cellSize {
  [_titleLabel setText:tab.title];
  [self contentView].accessibilityLabel = tab.title;
  if (tab.favicon) {
    [_favicon setImage:tab.favicon];
  } else {
    [_favicon setImage:NativeImage(IDR_IOS_OMNIBOX_HTTP)];
  }

  CGSize snapshotSize = cellSize;
  snapshotSize.height -= tabSwitcherLocalSessionCellTopBarHeight();
  base::WeakNSObject<TabSwitcherLocalSessionCell> weakCell(self);
  DCHECK(self.delegate);
  DCHECK([self cache]);
  _currentPendingSnapshotRequest =
      [[self cache] requestSnapshotForTab:tab
                                 withSize:snapshotSize
                          completionBlock:^(UIImage* image) {
                            DCHECK([NSThread isMainThread]);
                            [weakCell setSnapshot:image];
                            _currentPendingSnapshotRequest = {};
                          }];
}

- (void)setAppearanceForTabTitle:(NSString*)title
                         favicon:(UIImage*)favicon
                        cellSize:(CGSize)cellSize {
  [_titleLabel setText:title];
  [_snapshotButton setAccessibilityIdentifier:
                      [NSString stringWithFormat:@"%@_button", title]];
  [self contentView].accessibilityLabel = title;
  if (favicon) {
    [_favicon setImage:favicon];
  } else {
    [_favicon setImage:NativeImage(IDR_IOS_OMNIBOX_HTTP)];
  }

  // PLACEHOLDER: Set snapshot here.
}

- (void)setSessionType:(TabSwitcherSessionType)type {
  UIColor* topBarBackgroundColor;
  UIColor* closeButtonTintColor;
  UIColor* textColor;
  UIColor* snapshotBackgroundColor;
  if (type == TabSwitcherSessionType::OFF_THE_RECORD_SESSION) {
    topBarBackgroundColor = [[MDCPalette greyPalette] tint700];
    closeButtonTintColor = [[MDCPalette greyPalette] tint100];
    textColor = [[MDCPalette greyPalette] tint100];
    snapshotBackgroundColor = [[MDCPalette greyPalette] tint900];
  } else {
    topBarBackgroundColor = [[MDCPalette greyPalette] tint100];
    closeButtonTintColor = [[MDCPalette greyPalette] tint700];
    textColor = [[MDCPalette greyPalette] tint700];
    snapshotBackgroundColor = [UIColor whiteColor];
  }
  [_topBar setBackgroundColor:topBarBackgroundColor];
  [[_closeButton imageView] setTintColor:closeButtonTintColor];
  [_titleLabel setTextColor:textColor];
  [_titleLabel setBackgroundColor:topBarBackgroundColor];
  [_snapshot setBackgroundColor:snapshotBackgroundColor];
}

- (void)snapshotPressed {
  [self.delegate cellPressed:self];
}

- (void)closeButtonPressed {
  [self.delegate deleteButtonPressedForCell:self];
}

- (void)prepareForReuse {
  [[self cache] cancelPendingSnapshotRequest:_currentPendingSnapshotRequest];
  _currentPendingSnapshotRequest.clear();
  [_snapshot setImage:nil];
  [_snapshotButton resetState];
  [super prepareForReuse];
}

- (TabSwitcherCache*)cache {
  return [self.delegate tabSwitcherCache];
}

#pragma mark - UIAccessibilityAction

- (NSArray*)accessibilityCustomActions {
  base::scoped_nsobject<NSMutableArray> customActions(
      [[NSMutableArray alloc] init]);
  base::scoped_nsobject<UIAccessibilityCustomAction> customAction(
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CLOSE_TAB)
                target:self
              selector:@selector(closeButtonPressed)]);
  [customActions addObject:customAction.autorelease()];
  return customActions.autorelease();
}

@end

@implementation TabSwitcherDistantSessionCell {
  base::scoped_nsobject<UILabel> _titleLabel;
  base::scoped_nsobject<UIImageView> _favicon;
  base::scoped_nsobject<UIImageView> _newTabIcon;
  base::scoped_nsobject<UIView> _verticallyCenteredView;
  base::scoped_nsobject<TabSwitcherButton> _raisedButton;
  base::scoped_nsobject<NSOperation> _faviconObtainer;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Create and add the button that contains all other subviews.
    _raisedButton.reset([[TabSwitcherButton alloc] initWithFrame:CGRectZero]);
    [_raisedButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_raisedButton addTarget:self
                      action:@selector(cellPressed)
            forControlEvents:UIControlEventTouchUpInside];
    [[self containerView] addSubview:_raisedButton];
    ApplyVisualConstraints(@[ @"H:|-0-[button]-0-|", @"V:|-0-[button]-0-|" ],
                           @{ @"button" : _raisedButton.get() },
                           [self containerView]);

    // Create and add view that will be vertically centered in the space over
    // the favicon.
    _verticallyCenteredView.reset([[UIView alloc] initWithFrame:CGRectZero]);
    [_verticallyCenteredView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_verticallyCenteredView setUserInteractionEnabled:NO];
    [_raisedButton addSubview:_verticallyCenteredView];

    // Create and add title label to |_verticallyCenteredContent|.
    _titleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_titleLabel setNumberOfLines:5];
    [_titleLabel setTextAlignment:NSTextAlignmentCenter];
    [_titleLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                             regularFontOfSize:kFontSize]];
    [_verticallyCenteredView addSubview:_titleLabel];

    // Create and add new tab icon to |_verticallyCenteredContent|.
    UIImage* newTabIcon = [[UIImage imageNamed:@"tabswitcher_new_tab"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _newTabIcon.reset([[UIImageView alloc] initWithImage:newTabIcon]);
    [_newTabIcon setAlpha:0];
    [_newTabIcon setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_verticallyCenteredView addSubview:_newTabIcon];

    // Create and add favicon image container.
    _favicon.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [_favicon setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_raisedButton addSubview:_favicon];

    // Add constraints to the button's subviews.
    NSDictionary* viewsDictionary = @{
      @"newTabIcon" : _newTabIcon.get(),
      @"title" : _titleLabel.get(),
      @"favicon" : _favicon.get(),
      @"centeredView" : _verticallyCenteredView.get(),
    };
    NSArray* constraintsInButton = @[
      @"H:|-0-[centeredView]-0-|",
      @"H:[favicon(==16)]",
      @"V:|-(>=16)-[centeredView]-(>=16)-[favicon(==16)]-16-|",
    ];
    ApplyVisualConstraints(constraintsInButton, viewsDictionary, _raisedButton);
    AddSameCenterXConstraint(_raisedButton, _favicon.get());
    [_raisedButton
        addConstraint:[NSLayoutConstraint
                          constraintWithItem:_verticallyCenteredView.get()
                                   attribute:NSLayoutAttributeCenterY
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:_favicon.get()
                                   attribute:NSLayoutAttributeCenterY
                                  multiplier:0.5
                                    constant:0]];

    // Add constraints to the subviews of the vertically centered view.
    NSArray* constraintsInVerticallyCenteredView = @[
      @"H:|-16-[title]-16-|",
      @"V:|-0-[newTabIcon(==24)]-16-[title(>=16)]-0-|",
    ];
    ApplyVisualConstraints(constraintsInVerticallyCenteredView, viewsDictionary,
                           _verticallyCenteredView);
    AddSameCenterXConstraint(_verticallyCenteredView, _newTabIcon.get());
  }
  return self;
}

- (void)setTitle:(NSString*)titleString {
  [_titleLabel setText:titleString];
  [self contentView].accessibilityLabel = titleString;
}

- (void)setSessionGURL:(GURL const&)gurl
      withBrowserState:(ios::ChromeBrowserState*)browserState {
  TabSwitcherFaviconGetterCompletionBlock block = ^(UIImage* favicon) {
    UIColor* imageDominantColor =
        DominantColorForImage(gfx::Image(favicon), 1.0);
    MDCPalette* dominantPalette =
        [MDCPalette paletteGeneratedFromColor:imageDominantColor];
    UIColor* backgroundColor = dominantPalette.tint300;
    UIColor* textColor =
        [MDFTextAccessibility textColorOnBackgroundColor:backgroundColor
                                         targetTextAlpha:kTitleLabelTextAlpha
                                                    font:[_titleLabel font]];
    UIColor* iconColor =
        [MDFTextAccessibility textColorOnBackgroundColor:backgroundColor
                                         targetTextAlpha:kNewTabIconAlpha
                                                    font:[_titleLabel font]];
    [_raisedButton setBackgroundColor:backgroundColor];
    [_titleLabel setTextColor:textColor];
    [_newTabIcon setTintColor:iconColor];
    [_newTabIcon setAlpha:1.0];
    [_favicon setImage:favicon];
    [UIView animateWithDuration:0.2
                     animations:^{
                       [_raisedButton setAlpha:1.0];
                     }];
  };
  GURL gurlCopy = gurl;
  _faviconObtainer.reset([[NSBlockOperation blockOperationWithBlock:^{
    TabSwitcherGetFavicon(gurlCopy, browserState, block);
  }] retain]);
  NSOperationQueue* operationQueue = [NSOperationQueue mainQueue];
  [operationQueue addOperation:_faviconObtainer];
}

- (void)cellPressed {
  [self.delegate cellPressed:self];
}

- (void)prepareForReuse {
  [_newTabIcon setAlpha:0];
  [_faviconObtainer cancel];
  _faviconObtainer.reset();
  [_raisedButton setAlpha:0];
  [_raisedButton resetState];
  [super prepareForReuse];
}

@end
