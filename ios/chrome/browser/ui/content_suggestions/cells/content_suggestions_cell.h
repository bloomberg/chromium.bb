// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELL_H_

#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class FaviconViewNew;

// Corresponding cell for an article in the suggestions.
@interface ContentSuggestionsCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UILabel* titleLabel;
// View for displaying the favicon.
@property(nonatomic, readonly, strong) FaviconViewNew* faviconView;
// Whether the image should be displayed.
@property(nonatomic, assign) BOOL displayImage;

// Sets an |image| to illustrate the article, replacing the "no image" icon.
- (void)setContentImage:(UIImage*)image;

// Sets the publisher |name| and |date| and add an icon to signal the offline
// availability if |availableOffline| is YES.
- (void)setAdditionalInformationWithPublisherName:(NSString*)publisherName
                                             date:(NSDate*)publishDate
                              offlineAvailability:(BOOL)availableOffline;

// Setst the subtitle text. If |subtitle| is nil, the space taken by the
// subtitle is removed.
- (void)setSubtitleText:(NSString*)subtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELL_H_
