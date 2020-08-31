// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_H_

#import <UIKit/UIKit.h>

// States in which the language tab can be.
typedef NS_ENUM(NSInteger, TranslateInfobarLanguageTabViewState) {
  TranslateInfobarLanguageTabViewStateDefault,
  TranslateInfobarLanguageTabViewStateSelected,
  TranslateInfobarLanguageTabViewStateLoading,
};

@protocol TranslateInfobarLanguageTabViewDelegate;

// The language tab view featuring a label. It can be in a default, selected, or
// loading state. In the selected state the label is highlighted and in the
// loading state the label is replaced by an activity indicator.
@interface TranslateInfobarLanguageTabView : UIView

// Title of the language tab.
@property(nonatomic, copy) NSString* title;

// State of the language tab.
@property(nonatomic) TranslateInfobarLanguageTabViewState state;

// Delegate object that gets notified if user taps the view.
@property(nonatomic, weak) id<TranslateInfobarLanguageTabViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_H_
