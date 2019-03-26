// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;

namespace {
NSString* OmniboxPopupRowCellReuseIdentifier = @"OmniboxPopupRowCell";
}  // namespace

// Table view cell to display an autocomplete suggestion in the omnibox popup.
// It handles all the layout logic internally.
@interface OmniboxPopupRowCell : UITableViewCell

- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ROW_CELL_H_
