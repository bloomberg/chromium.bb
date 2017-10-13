// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_MOST_VISITED_CELL_H_
#define IOS_CHROME_BROWSER_UI_NTP_MOST_VISITED_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "url/gurl.h"

@protocol GoogleLandingDataSource;

// Cell showing each most visited image, favicon and title.
@interface MostVisitedCell : UICollectionViewCell

// URL of the top site.
@property(nonatomic, assign) GURL URL;
// Type of tile (icon, scrabble tile, default)
@property(nonatomic, readonly) FaviconAttributes* faviconAttributes;

// Set text of top site.
- (void)setText:(NSString*)text;
// Show the placeholder image in the cell instead of a most visited site.
- (void)showPlaceholder;
// Removes the placeholder image from the cell. Used to make a cell appear
// empty/invisible.
- (void)removePlaceholderImage;
// Setup the display state of the cell.
- (void)setupWithURL:(GURL)URL
               title:(NSString*)title
          dataSource:(id<GoogleLandingDataSource>)dataSource;

// Preferred maximum cell size.
+ (CGSize)maximumSize;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_MOST_VISITED_CELL_H_
