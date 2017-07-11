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
- (void)setContentImage:(UIImage*)image animated:(BOOL)animated;

// Sets the publisher |name| and |date| and add an icon to signal the offline
// availability if |availableOffline| is YES.
- (void)setAdditionalInformationWithPublisherName:(NSString*)publisherName
                                             date:(NSString*)publishDate
                              offlineAvailability:(BOOL)availableOffline;

// Returns the height needed by a cell contained in |width| and containing the
// listed informations.
+ (CGFloat)heightForWidth:(CGFloat)width
                withImage:(BOOL)hasImage
                    title:(NSString*)title
            publisherName:(NSString*)publisherName
          publicationDate:(NSString*)publicationDate
         availableOffline:(BOOL)availableOffline;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELL_H_
