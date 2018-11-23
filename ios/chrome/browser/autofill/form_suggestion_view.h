// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

@class FormSuggestion;
@protocol FormSuggestionClient;

// A scrollable view for displaying user-selectable autofill form suggestions.
@interface FormSuggestionView : UIScrollView<UIInputViewAudioFeedback>

// The current suggestions this view is showing.
@property(nonatomic, readonly) NSArray<FormSuggestion*>* suggestions;

// A view added at the end of the current suggestions.
@property(nonatomic, strong) UIView* trailingView;

// Updates with |client| and |suggestions|.
- (void)updateClient:(id<FormSuggestionClient>)client
         suggestions:(NSArray<FormSuggestion*>*)suggestions;

// Animates the content insets back to zero.
- (void)unlockTrailingView;

// Animates the content insets so the trailing view is showed as the first
// thing.
- (void)lockTrailingView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
