// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_SAVER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_SAVER_H_

#import <Foundation/Foundation.h>

#include "components/ntp_tiles/ntp_tile.h"

@protocol GoogleLandingDataSource;
@class NTPTile;

// These functions are used to save the ntp tiles (favicon and name) offline for
// the use of the content widget. The most visited info and icon fallback data
// are saved to userdefaults. The favicons are saved to a shared folder. Because
// the favicons are fetched asynchronously, they are first saved in a temporary
// folder which replaces the current favicons when all are fetched.
namespace ntp_tile_saver {

// Saves the most visited sites to disk with icons in |faviconsURL|, using
// |faviconFetcher| to get the favicons.
void SaveMostVisitedToDisk(const ntp_tiles::NTPTilesVector& mostVisitedData,
                           id<GoogleLandingDataSource> faviconFetcher,
                           NSURL* faviconsURL);

// Read the current saved most visited sites from disk.
NSDictionary* ReadSavedMostVisited();

// Fetches the updated favicon for a single site and saves it in |faviconsURL|.
void UpdateSingleFavicon(const GURL& siteURL,
                         id<GoogleLandingDataSource> faviconFetcher,
                         NSURL* faviconsURL);

}  // namespace ntp_tile_saver

#endif  // IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_SAVER_H_
