// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_UI_ELEMENT_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_UI_ELEMENT_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/800266): Merge this into FindBarView.
// Protocol for the UI Element of FindInPage. This protocol allows the new and
// old UI to share a common interface.
@protocol FindBarUIElement

// Updates |resultsLabel| with |text|. Updates |inputField| layout so that input
// text does not overlap with results count. |text| can be nil.
- (void)updateResultsLabelWithText:(NSString*)text;

// The textfield with search term.
@property(nonatomic, weak) UITextField* inputField;
// Button to go to previous search result.
@property(nonatomic, weak) UIButton* previousButton;
// Button to go to next search result.
@property(nonatomic, weak) UIButton* nextButton;
// Button to dismiss Find in Page.
@property(nonatomic, weak) UIButton* closeButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_UI_ELEMENT_H_
