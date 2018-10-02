// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"

#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_view.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kFindInPageContainerViewId = @"kFindInPageContainerViewId";

namespace {

const CGFloat kFindBarWidthRegularRegular = 375;
const CGFloat kFindBarCornerRadiusRegularRegular = 13;
const CGFloat kRegularRegularHorizontalMargin = 5;

// Margin between the beginning of the shadow image and the content being
// shadowed.
const CGFloat kShadowMargin = 196;

// Find Bar animation drop down duration.
const CGFloat kAnimationDuration = 0.15;

// For the first |kSearchDelayChars| characters, delay by |kSearchLongDelay|
// For the remaining characters, delay by |kSearchShortDelay|.
const NSUInteger kSearchDelayChars = 3;
const NSTimeInterval kSearchLongDelay = 1.0;
const NSTimeInterval kSearchShortDelay = 0.100;

}  // anonymous namespace

#pragma mark - FindBarControllerIOS

@interface FindBarControllerIOS ()<UITextFieldDelegate>

// In legacy UI: animates find bar to iPad top right, or, when possible, to
// align find bar horizontally with |alignmentFrame|. In new UI: animates find
// bar to the right of |parentView| and below |toolbarView|.
- (void)showIPadFindBarViewInParentView:(UIView*)parentView
                       usingToolbarView:(UIView*)toolbarView
                             selectText:(BOOL)selectText
                               animated:(BOOL)animated;
// Animate find bar over iPhone toolbar.
- (void)showIPhoneFindBarViewInParentView:(UIView*)parentView
                               selectText:(BOOL)selectText
                                 animated:(BOOL)animated;
// Responds to touches that make editing changes on the text field, triggering
// find-in-page searches for the field's current value.
- (void)editingChanged;
// Return the expected find bar height. This will include the status bar height
// when running iOS7 on an iPhone.
- (CGFloat)findBarHeight;
// Selects text in such way that selection menu does not appear and
// a11y label is read. When -[UITextField selectAll:] is used, iOS
// will read "Select All" instead of a11y label.
- (void)selectAllText;

// Redefined to be readwrite. This view acts as background for |findBarView| and
// contains it as a subview.
@property(nonatomic, readwrite, strong) UIView* view;
// The view containing all the buttons and textfields that is common between
// iPhone and iPad.
@property(nonatomic, strong) FindBarView* findBarView;
// Typing delay timer.
@property(nonatomic, strong) NSTimer* delayTimer;
// Yes if incognito.
@property(nonatomic, assign) BOOL isIncognito;
@end

@implementation FindBarControllerIOS {
  // This var is only useful for iPhone in iOS 10.
  NSLayoutConstraint* _heightConstraint;
}

@synthesize view = _view;
@synthesize findBarView = _findBarView;
@synthesize delayTimer = _delayTimer;
@synthesize isIncognito = _isIncognito;
@synthesize dispatcher = _dispatcher;

#pragma mark - Lifecycle

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
  }
  return self;
}

#pragma mark View Setup & Teardown

- (UIView*)constructFindBarView {
  UIView* findBarBackground = nil;

  findBarBackground = [[UIView alloc] initWithFrame:CGRectZero];
  findBarBackground.backgroundColor =
      self.isIncognito ? UIColorFromRGB(kIncognitoToolbarBackgroundColor)
                       : UIColorFromRGB(kToolbarBackgroundColor);
  self.findBarView =
      [[FindBarView alloc] initWithDarkAppearance:self.isIncognito];

  [findBarBackground addSubview:self.findBarView];
  self.findBarView.translatesAutoresizingMaskIntoConstraints = NO;

  CGFloat findBarHeight = kAdaptiveToolbarHeight;

  NSMutableArray* constraints = [[NSMutableArray alloc] init];
  [constraints addObjectsFromArray:@[
    [self.findBarView.trailingAnchor
        constraintEqualToAnchor:findBarBackground.trailingAnchor],
    [self.findBarView.leadingAnchor
        constraintEqualToAnchor:findBarBackground.leadingAnchor],
    [self.findBarView.heightAnchor constraintEqualToConstant:findBarHeight]
  ]];

  [constraints
      addObject:[self.findBarView.bottomAnchor
                    constraintEqualToAnchor:findBarBackground.bottomAnchor]];

  [NSLayoutConstraint activateConstraints:constraints];

  self.findBarView.inputField.delegate = self;
  [self.findBarView.inputField addTarget:self
                                  action:@selector(editingChanged)
                        forControlEvents:UIControlEventEditingChanged];
  [self.findBarView.nextButton addTarget:self.dispatcher
                                  action:@selector(findNextStringInPage)
                        forControlEvents:UIControlEventTouchUpInside];
  [self.findBarView.nextButton addTarget:self
                                  action:@selector(hideKeyboard:)
                        forControlEvents:UIControlEventTouchUpInside];
  [self.findBarView.previousButton addTarget:self.dispatcher
                                      action:@selector(findPreviousStringInPage)
                            forControlEvents:UIControlEventTouchUpInside];
  [self.findBarView.previousButton addTarget:self
                                      action:@selector(hideKeyboard:)
                            forControlEvents:UIControlEventTouchUpInside];
  [self.findBarView.closeButton addTarget:self.dispatcher
                                   action:@selector(closeFindInPage)
                         forControlEvents:UIControlEventTouchUpInside];

  return findBarBackground;
}

- (void)teardownView {
  [self.view removeFromSuperview];
  self.view = nil;
}

#pragma mark - Public

- (NSString*)searchTerm {
  return [self.findBarView.inputField text];
}

- (BOOL)isFindInPageShown {
  return self.view != nil;
}

- (BOOL)isFocused {
  return [self.findBarView.inputField isFirstResponder];
}

- (void)updateResultsCount:(FindInPageModel*)model {
  [self updateWithMatchNumber:model.currentIndex
                   matchCount:model.matches
                   searchText:model.text];
}

- (void)updateView:(FindInPageModel*)model
     initialUpdate:(BOOL)initialUpdate
    focusTextfield:(BOOL)focusTextfield {
  [self.delayTimer invalidate];
  self.delayTimer = nil;

  if (initialUpdate) {
    // Set initial text and first search.
    [self.findBarView.inputField setText:model.text];
    [self editingChanged];
  }

  // Focus input field if necessary.
  if (focusTextfield) {
    [self.findBarView.inputField becomeFirstResponder];
  } else {
    [self.findBarView.inputField resignFirstResponder];
  }

  [self updateWithMatchNumber:model.currentIndex
                   matchCount:model.matches
                   searchText:model.text];
}

- (void)updateWithMatchNumber:(NSUInteger)matchNumber
                   matchCount:(NSUInteger)matchCount
                   searchText:(NSString*)searchText {
  NSString* text = nil;
  if (searchText.length != 0) {
    NSString* indexStr = [NSString stringWithFormat:@"%" PRIdNS, matchNumber];
    NSString* matchesStr = [NSString stringWithFormat:@"%" PRIdNS, matchCount];
    text = l10n_util::GetNSStringF(IDS_FIND_IN_PAGE_COUNT,
                                   base::SysNSStringToUTF16(indexStr),
                                   base::SysNSStringToUTF16(matchesStr));
  }
  [self.findBarView updateResultsLabelWithText:text];

  BOOL enabled = matchCount != 0;
  self.findBarView.nextButton.enabled = enabled;
  self.findBarView.previousButton.enabled = enabled;
}

- (void)addFindBarViewToParentView:(UIView*)parentView
                  usingToolbarView:(UIView*)toolbarView
                        selectText:(BOOL)selectText
                          animated:(BOOL)animated {
  // If already showing find bar, update the height constraint only for iOS 10
  // because the safe area anchor is not available.
  if (self.view) {
    if (@available(iOS 11, *)) {
    } else if (ShouldShowCompactToolbar()) {
      _heightConstraint.constant = [self findBarHeight];
    }
    return;
  }

  if (ShouldShowCompactToolbar()) {
    [self showIPhoneFindBarViewInParentView:parentView
                                 selectText:selectText
                                   animated:animated];
  } else {
    [self showIPadFindBarViewInParentView:parentView
                         usingToolbarView:toolbarView
                               selectText:selectText
                                 animated:animated];
  }
}

- (void)hideFindBarView:(BOOL)animate {
  // If view is nil, nothing to hide.
  if (!self.view) {
    return;
  }

  self.findBarView.inputField.selectedTextRange = nil;
  [self.delayTimer invalidate];
  self.delayTimer = nil;

  if (animate) {
    CGRect oldFrame = self.view.frame;
    self.view.layer.anchorPoint = CGPointMake(0.5, 0);
    self.view.frame = oldFrame;
    [UIView animateWithDuration:kAnimationDuration
        animations:^{
          self.view.transform = CGAffineTransformMakeScale(1, 0.05);
          self.view.alpha = 0;
        }
        completion:^(BOOL finished) {
          [self teardownView];
        }];
  } else {
    [self teardownView];
  }
}

- (void)hideKeyboard:(id)sender {
  [self.view endEditing:YES];
}

#pragma mark - Internal

- (void)selectAllText {
  UITextRange* wholeTextRange = [self.findBarView.inputField
      textRangeFromPosition:self.findBarView.inputField.beginningOfDocument
                 toPosition:self.findBarView.inputField.endOfDocument];
  self.findBarView.inputField.selectedTextRange = wholeTextRange;
}

// Animate find bar to iPad top right.
- (void)showIPadFindBarViewInParentView:(UIView*)parentView
                       usingToolbarView:(UIView*)toolbarView
                             selectText:(BOOL)selectText
                               animated:(BOOL)animated {
  DCHECK(IsIPadIdiom());
  self.view = [self constructFindBarView];
  self.view.accessibilityIdentifier = kFindInPageContainerViewId;

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.layer.cornerRadius = kFindBarCornerRadiusRegularRegular;
  [parentView addSubview:self.view];

  UIImageView* shadow =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  shadow.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:shadow];

  CGFloat parentWidth = CGRectGetWidth(parentView.bounds);
  CGFloat width = MIN(parentWidth - 2 * kRegularRegularHorizontalMargin,
                      kFindBarWidthRegularRegular);
  [NSLayoutConstraint activateConstraints:@[
    // Anchors findbar below |toolbarView|.
    [self.view.topAnchor constraintEqualToAnchor:toolbarView.bottomAnchor],
    // Aligns findbar with the right side of |parentView|.
    [self.view.trailingAnchor
        constraintEqualToAnchor:parentView.trailingAnchor
                       constant:-kRegularRegularHorizontalMargin],
    [self.view.widthAnchor constraintEqualToConstant:width],
    [self.view.heightAnchor constraintEqualToConstant:kAdaptiveToolbarHeight],
  ]];
  // Layouts |shadow| around |self.view|.
  AddSameConstraintsToSidesWithInsets(
      shadow, self.view,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kBottom |
          LayoutSides::kTrailing,
      {-kShadowMargin, -kShadowMargin, -kShadowMargin, -kShadowMargin});

  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(1, 0.05);
  CGFloat duration = animated ? kAnimationDuration : 0;
  [UIView animateWithDuration:duration
      animations:^() {
        self.view.alpha = 1;
        self.view.transform = CGAffineTransformIdentity;
      }
      completion:^(BOOL finished) {
        if (selectText)
          [self selectAllText];
      }];
}

// Animate find bar over iPhone toolbar.
- (void)showIPhoneFindBarViewInParentView:(UIView*)parentView
                               selectText:(BOOL)selectText
                                 animated:(BOOL)animated {
  self.view = [self constructFindBarView];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [parentView addSubview:self.view];
  self.view.accessibilityIdentifier = kFindInPageContainerViewId;

  NSLayoutConstraint* leadingConstraint = [self.view.leadingAnchor
      constraintEqualToAnchor:parentView.leadingAnchor];
  NSLayoutConstraint* trailingConstraint = [self.view.trailingAnchor
      constraintEqualToAnchor:parentView.trailingAnchor];
  // Under iOS 10 SafeArea anchor is not available, so findbar is anchored to
  // the top of screen and the height of statusbar should be considered.
  NSLayoutConstraint* topConstraint = [self.view.topAnchor
      constraintEqualToAnchor:SafeAreaLayoutGuideForView(parentView).topAnchor];

  CGFloat height;
  if (@available(iOS 11, *)) {
    height = kAdaptiveToolbarHeight;
  } else {
    // Portrait:  height = findbar + statusbar.
    // Landscape: height = findbar
    // The height constraint should be updated when iOS 10 device rotates.
    height = [self findBarHeight];
  }
  _heightConstraint = [self.view.heightAnchor constraintEqualToConstant:height];
  [NSLayoutConstraint activateConstraints:@[
    leadingConstraint, trailingConstraint, topConstraint, _heightConstraint
  ]];

  [self.view setTransform:CGAffineTransformMakeTranslation(0, -height)];
  CGFloat duration = animated ? kAnimationDuration : 0;
  [UIView animateWithDuration:duration
      animations:^() {
        [self.view setTransform:CGAffineTransformIdentity];
      }
      completion:^(BOOL finished) {
        if (selectText)
          [self selectAllText];
      }];
}

- (void)editingChanged {
  [self.delayTimer invalidate];
  NSUInteger length = [[self searchTerm] length];
  if (length == 0) {
    [self.dispatcher searchFindInPage];
    return;
  }

  // Delay delivery of the search text event to give time for a user to type out
  // a longer word.  Use a longer delay when the input length is short, as short
  // words are currently very inefficient and lock up the web view.
  NSTimeInterval delay =
      (length > kSearchDelayChars) ? kSearchShortDelay : kSearchLongDelay;
  self.delayTimer =
      [NSTimer scheduledTimerWithTimeInterval:delay
                                       target:self.dispatcher
                                     selector:@selector(searchFindInPage)
                                     userInfo:nil
                                      repeats:NO];
}

- (CGFloat)findBarHeight {
  if (IsRegularXRegularSizeClass())
    return kAdaptiveToolbarHeight;
  return StatusBarHeight() + kAdaptiveToolbarHeight;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldBeginEditing:(UITextField*)textField {
  DCHECK(textField == self.findBarView.inputField);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kFindBarTextFieldWillBecomeFirstResponderNotification
                    object:self];
  return YES;
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  DCHECK(textField == self.findBarView.inputField);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kFindBarTextFieldDidResignFirstResponderNotification
                    object:self];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK(textField == self.findBarView.inputField);
  [self.findBarView.inputField resignFirstResponder];
  return YES;
}

@end
