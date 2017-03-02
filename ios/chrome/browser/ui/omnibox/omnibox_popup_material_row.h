// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_ROW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_ROW_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupTruncatingLabel;

// View used to display an omnibox autocomplete match in the omnibox popup.
@interface OmniboxPopupMaterialRow : UITableViewCell

// A truncate-by-fading version of the textLabel of a UITableViewCell.
@property(nonatomic, readonly, retain)
    OmniboxPopupTruncatingLabel* textTruncatingLabel;
// A truncate-by-fading version of the detailTextLabel of a UITableViewCell.
@property(nonatomic, readonly, retain)
    OmniboxPopupTruncatingLabel* detailTruncatingLabel;
// A standard UILabel for answers, which truncates with ellipses to support
// multi-line text.
@property(nonatomic, readonly, retain) UILabel* detailAnswerLabel;

@property(nonatomic, readonly, retain) UIImageView* imageView;
@property(nonatomic, readonly, retain) UIImageView* answerImageView;
@property(nonatomic, readonly, retain) UIButton* appendButton;
@property(nonatomic, assign) CGFloat rowHeight;

// Initialize the row with the given incognito state. The colors and styling are
// dependent on whether or not the row is displayed in incognito mode.
- (instancetype)initWithIncognito:(BOOL)incognito;

// Update the match type icon with the supplied image ID and adjust its position
// based on the current size of the row.
- (void)updateLeadingImage:(int)imageID;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MATERIAL_ROW_H_
