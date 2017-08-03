// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/before_translate_infobar_controller.h"

#include <stddef.h>
#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kLanguagePickerCancelButtonId = @"LanguagePickerCancelButton";
NSString* const kLanguagePickerDoneButtonId = @"LanguagePickerDoneButton";

namespace {

CGFloat kNavigationBarHeight = 44;
CGFloat kUIPickerHeight = 216;
CGFloat kUIPickerFontSize = 26;
NSTimeInterval kPickerAnimationDurationInSeconds = 0.2;

}  // namespace

// The class is a data source and delegate to the UIPickerView that contains the
// language list.
@interface LanguagePickerController
    : UIViewController<UIPickerViewDataSource, UIPickerViewDelegate> {
  translate::TranslateInfoBarDelegate* _translateInfoBarDelegate;  // weak
  NSInteger _initialRow;   // Displayed in bold font.
  NSInteger _disabledRow;  // Grayed out.
}
@end

@implementation LanguagePickerController

- (instancetype)initWithDelegate:(translate::TranslateInfoBarDelegate*)
                                     translateInfoBarDelegate
                      initialRow:(NSInteger)initialRow
                     disabledRow:(NSInteger)disabledRow {
  if ((self = [super init])) {
    _translateInfoBarDelegate = translateInfoBarDelegate;
    _initialRow = initialRow;
    _disabledRow = disabledRow;
  }
  return self;
}

#pragma mark -
#pragma mark UIPickerViewDataSource

- (NSInteger)pickerView:(UIPickerView*)pickerView
    numberOfRowsInComponent:(NSInteger)component {
  NSUInteger numRows = _translateInfoBarDelegate->num_languages();
  return numRows;
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView*)pickerView {
  return 1;
}

#pragma mark -
#pragma mark UIPickerViewDelegate

- (UIView*)pickerView:(UIPickerView*)pickerView
           viewForRow:(NSInteger)row
         forComponent:(NSInteger)component
          reusingView:(UIView*)view {
  DCHECK_EQ(0, component);
  UILabel* label = [view isKindOfClass:[UILabel class]]
                       ? (UILabel*)view
                       : [[UILabel alloc] init];
  [label setText:base::SysUTF16ToNSString(
                     _translateInfoBarDelegate->language_name_at(row))];
  [label setTextAlignment:NSTextAlignmentCenter];
  UIFont* font = [UIFont systemFontOfSize:kUIPickerFontSize];
  BOOL enabled = YES;
  if (row == _initialRow)
    font = [UIFont boldSystemFontOfSize:kUIPickerFontSize];
  else if (row == _disabledRow)
    enabled = NO;
  [label setFont:font];
  [label setEnabled:enabled];
  return label;
}

@end

@interface BeforeTranslateInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;
// Action for any of the user defined links.
- (void)infobarLinkDidPress:(NSUInteger)tag;
// Action for the language selection "Done" button.
- (void)languageSelectionDone;
// Dismisses the language selection view.
- (void)dismissLanguageSelectionView;
// Changes the text on the view to match the language.
- (void)updateInfobarLabelOnView:(InfoBarView*)view;

@end

@implementation BeforeTranslateInfoBarController {
  translate::TranslateInfoBarDelegate* _translateInfoBarDelegate;  // weak
  // A fullscreen view that catches all touch events and contains a UIPickerView
  // and a UINavigationBar.
  UIView* _languageSelectionView;
  // Stores whether the user is currently choosing in the UIPickerView the
  // original language, or the target language.
  NSUInteger _languageSelectionType;
  // The language picker.
  UIPickerView* _languagePicker;
  // Navigation bar associated with the picker with "Done" and "Cancel" buttons.
  UINavigationBar* _navigationBar;
  // The controller of the languagePicker. Needs to be an ivar because
  // |_languagePicker| does not retain it.
  LanguagePickerController* _languagePickerController;
}

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (InfoBarView*)viewForDelegate:(infobars::InfoBarDelegate*)delegate
                          frame:(CGRect)frame {
  InfoBarView* infoBarView;
  _translateInfoBarDelegate = delegate->AsTranslateInfoBarDelegate();
  infoBarView =
      [[InfoBarView alloc] initWithFrame:frame delegate:self.delegate];
  // Icon
  gfx::Image icon = _translateInfoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];

  // Main text.
  [self updateInfobarLabelOnView:infoBarView];

  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::BEFORE_DENY
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonAccept = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_ACCEPT);
  NSString* buttonDeny = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_DENY);
  [infoBarView addButton1:buttonAccept
                     tag1:TranslateInfoBarIOSTag::BEFORE_ACCEPT
                  button2:buttonDeny
                     tag2:TranslateInfoBarIOSTag::BEFORE_DENY
                   target:self
                   action:@selector(infoBarButtonDidPress:)];
  return infoBarView;
}

- (void)updateInfobarLabelOnView:(InfoBarView*)view {
  NSString* originalLanguage = base::SysUTF16ToNSString(
      _translateInfoBarDelegate->original_language_name());
  NSString* targetLanguage = base::SysUTF16ToNSString(
      _translateInfoBarDelegate->target_language_name());
  base::string16 originalLanguageWithLink =
      base::SysNSStringToUTF16([[view class]
          stringAsLink:originalLanguage
                   tag:TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE]);
  base::string16 targetLanguageWithLink = base::SysNSStringToUTF16([[view class]
      stringAsLink:targetLanguage
               tag:TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE]);
  NSString* label =
      l10n_util::GetNSStringF(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE_IOS,
                              originalLanguageWithLink, targetLanguageWithLink);

  __weak BeforeTranslateInfoBarController* weakSelf = self;
  [view addLabel:label
          action:^(NSUInteger tag) {
            [weakSelf infobarLinkDidPress:tag];
          }];
}

- (void)languageSelectionDone {
  size_t selectedRow = [_languagePicker selectedRowInComponent:0];
  std::string lang = _translateInfoBarDelegate->language_code_at(selectedRow);
  if (_languageSelectionType ==
          TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE &&
      lang != _translateInfoBarDelegate->target_language_code()) {
    _translateInfoBarDelegate->UpdateOriginalLanguage(lang);
  }
  if (_languageSelectionType ==
          TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE &&
      lang != _translateInfoBarDelegate->original_language_code()) {
    _translateInfoBarDelegate->UpdateTargetLanguage(lang);
  }
  [self updateInfobarLabelOnView:self.view];
  [self dismissLanguageSelectionView];
}

- (void)dismissLanguageSelectionView {
  DCHECK_EQ(_languagePicker == nil, _navigationBar == nil);
  if (_languagePicker == nil)
    return;
  // Sets the picker's delegate and data source to nil, because the
  // |_languagePickerController| may be destroyed before the picker is hidden,
  // and even though the interactions with the picker are disabled, it might
  // still be turning and requesting data from the data source or calling the
  // delegate.
  [_languagePicker setDataSource:nil];
  [_languagePicker setDelegate:nil];
  _languagePickerController = nil;
  // Animate the picker away.
  CGRect languagePickerFrame = [_languagePicker frame];
  CGRect navigationBarFrame = [_navigationBar frame];
  const CGFloat animationHeight =
      languagePickerFrame.size.height + navigationBarFrame.size.height;
  languagePickerFrame.origin.y += animationHeight;
  navigationBarFrame.origin.y += animationHeight;
  UIPickerView* blockLanguagePicker = _languagePicker;
  UINavigationBar* blockNavigationBar = _navigationBar;
  _languagePicker = nil;
  _navigationBar = nil;
  void (^animations)(void) = ^{
    blockLanguagePicker.frame = languagePickerFrame;
    blockNavigationBar.frame = navigationBarFrame;
  };
  UIView* blockSelectionView = _languageSelectionView;
  _languageSelectionView = nil;
  void (^completion)(BOOL finished) = ^(BOOL finished) {
    [blockSelectionView removeFromSuperview];
  };
  [UIView animateWithDuration:kPickerAnimationDurationInSeconds
                   animations:animations
                   completion:completion];
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  // This press might have occurred after the user has already pressed a button,
  // in which case the view has been detached from the delegate and this press
  // should be ignored.
  if (!self.delegate) {
    return;
  }
  if ([sender isKindOfClass:[UIButton class]]) {
    NSUInteger buttonId = static_cast<UIButton*>(sender).tag;
    DCHECK(buttonId == TranslateInfoBarIOSTag::BEFORE_ACCEPT ||
           buttonId == TranslateInfoBarIOSTag::BEFORE_DENY);
    self.delegate->InfoBarButtonDidPress(buttonId);
  }
}

- (void)infobarLinkDidPress:(NSUInteger)tag {
  _languageSelectionType = tag;
  DCHECK(_languageSelectionType ==
             TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE ||
         _languageSelectionType ==
             TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE);
  if (_languagePicker != nil)
    return;  // The UIPickerView is already up.

  // Creates and adds the view containing the UIPickerView and the
  // UINavigationBar.
  UIView* parentView =
      [[UIApplication sharedApplication] keyWindow].rootViewController.view;
  // Convert the parent frame to handle device rotation.
  CGRect parentFrame =
      CGRectApplyAffineTransform([parentView frame], [parentView transform]);
  const CGFloat totalPickerHeight = kUIPickerHeight + kNavigationBarHeight;
  CGRect languageSelectionViewFrame =
      CGRectMake(0, parentFrame.size.height - totalPickerHeight,
                 parentFrame.size.width, totalPickerHeight);
  _languageSelectionView =
      [[UIView alloc] initWithFrame:languageSelectionViewFrame];
  [_languageSelectionView
      setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                          UIViewAutoresizingFlexibleTopMargin];
  [parentView addSubview:_languageSelectionView];

  // Creates the navigation bar and its buttons.
  CGRect finalPickerFrame = CGRectMake(
      0, [_languageSelectionView frame].size.height - kUIPickerHeight,
      [_languageSelectionView frame].size.width, kUIPickerHeight);
  CGRect finalNavigationBarFrame = CGRectMake(
      0, 0, [_languageSelectionView frame].size.width, kNavigationBarHeight);
  // The language picker animates from the bottom of the screen.
  CGRect initialPickerFrame = finalPickerFrame;
  initialPickerFrame.origin.y += totalPickerHeight;
  CGRect initialNavigationBarFrame = finalNavigationBarFrame;
  initialNavigationBarFrame.origin.y += totalPickerHeight;

  _navigationBar =
      [[UINavigationBar alloc] initWithFrame:initialNavigationBarFrame];
  const UIViewAutoresizing resizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
  [_navigationBar setAutoresizingMask:resizingMask];
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(languageSelectionDone)];
  [doneButton setAccessibilityIdentifier:kLanguagePickerDoneButtonId];
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissLanguageSelectionView)];
  [cancelButton setAccessibilityIdentifier:kLanguagePickerCancelButtonId];
  UINavigationItem* item = [[UINavigationItem alloc] initWithTitle:@""];
  [item setRightBarButtonItem:doneButton];
  [item setLeftBarButtonItem:cancelButton];
  [item setHidesBackButton:YES];
  [_navigationBar pushNavigationItem:item animated:NO];

  // Creates the PickerView and its controller.
  NSInteger selectedRow;
  NSInteger disabledRow;
  NSInteger originalLanguageIndex = -1;
  NSInteger targetLanguageIndex = -1;

  for (size_t i = 0; i < _translateInfoBarDelegate->num_languages(); ++i) {
    if (_translateInfoBarDelegate->language_code_at(i) ==
        _translateInfoBarDelegate->original_language_code()) {
      originalLanguageIndex = i;
    }
    if (_translateInfoBarDelegate->language_code_at(i) ==
        _translateInfoBarDelegate->target_language_code()) {
      targetLanguageIndex = i;
    }
  }
  DCHECK_GT(originalLanguageIndex, -1);
  DCHECK_GT(targetLanguageIndex, -1);

  if (_languageSelectionType ==
      TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE) {
    selectedRow = originalLanguageIndex;
    disabledRow = targetLanguageIndex;
  } else {
    selectedRow = targetLanguageIndex;
    disabledRow = originalLanguageIndex;
  }
  _languagePickerController = [[LanguagePickerController alloc]
      initWithDelegate:_translateInfoBarDelegate
            initialRow:selectedRow
           disabledRow:disabledRow];
  _languagePicker = [[UIPickerView alloc] initWithFrame:initialPickerFrame];
  [_languagePicker setAutoresizingMask:resizingMask];
  [_languagePicker setShowsSelectionIndicator:YES];
  [_languagePicker setDataSource:_languagePickerController];
  [_languagePicker setDelegate:_languagePickerController];
  [_languagePicker setShowsSelectionIndicator:YES];
  [_languagePicker setBackgroundColor:[self.view backgroundColor]];
  [_languagePicker selectRow:selectedRow inComponent:0 animated:NO];

  UIPickerView* blockLanguagePicker = _languagePicker;
  UINavigationBar* blockNavigationBar = _navigationBar;
  [UIView animateWithDuration:kPickerAnimationDurationInSeconds
                   animations:^{
                     blockLanguagePicker.frame = finalPickerFrame;
                     blockNavigationBar.frame = finalNavigationBarFrame;
                   }];

  // Add the subviews.
  [_languageSelectionView addSubview:_languagePicker];
  [_languageSelectionView addSubview:_navigationBar];
}

- (void)removeView {
  [super removeView];
  [self dismissLanguageSelectionView];
}

- (void)detachView {
  [super detachView];
  [self dismissLanguageSelectionView];
}

@end
