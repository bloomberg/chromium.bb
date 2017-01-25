// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_INTERNAL_CELL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_INTERNAL_CELL_H_

#import <UIKit/UIKit.h>

// Cell to display a favicon in the internal collection view.
@interface ContentSuggestionsFaviconInternalCell : UICollectionViewCell

@property(nonatomic, strong) UIImageView* faviconView;
@property(nonatomic, strong) UILabel* titleLabel;

+ (NSString*)reuseIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_INTERNAL_CELL_H_
