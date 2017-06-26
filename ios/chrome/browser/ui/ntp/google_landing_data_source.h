// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_DATA_SOURCE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "url/gurl.h"

class ReadingListModel;
class LargeIconCache;
namespace favicon {
class LargeIconService;
}

// DataSource for the google landing controller.
// TODO(crbug.com/694750): Most everything here can be moved to dispatcher.
@protocol GoogleLandingDataSource

// Removes a blacklisted URL in both |_mostVisitedData|.
- (void)removeBlacklistedURL:(const GURL&)url;

// Adds URL to the blacklist in both |_mostVisitedData|.
- (void)addBlacklistedURL:(const GURL&)url;

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedClick:(const NSUInteger)visitedIndex
                   tileType:(ntp_tiles::TileVisualType)tileType;

// Called when a what's new promo is viewed.
- (void)promoViewed;

// Called when a what's new promo is tapped.
- (void)promoTapped;

// TODO(crbug.com/694750): The following two methods should be moved to the
// consumer, and converted into types more suitable for a consumer.
// Gets an a most visited NTP tile at |index|.
- (ntp_tiles::NTPTile)mostVisitedAtIndex:(NSUInteger)index;

// Gets the number of most visited entries.
- (NSUInteger)mostVisitedSize;

// TODO(crbug.com/694750): The following three properties will be removed in
// subsequent CLs, with data provided via GoogleDataConsumer into types more
// suitable for a consumer.

// Gets the reading list model.
- (ReadingListModel*)readingListModel;

// Gets the large icon cache.
- (LargeIconCache*)largeIconCache;

// Gets the large icon service.
- (favicon::LargeIconService*)largeIconService;

// Asynchronously gets the favicon for |URL| with size |size| and calls
// imageCallback if the favicon is valid or fallbackCallback if there is valid
// fallback data.
- (void)getFaviconForURL:(GURL)URL
                    size:(CGFloat)size
                useCache:(BOOL)useCache
           imageCallback:(void (^)(UIImage*))imageCallback
        fallbackCallback:(void (^)(UIColor*, UIColor*, BOOL))fallbackCallback;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_DATA_SOURCE_H_
