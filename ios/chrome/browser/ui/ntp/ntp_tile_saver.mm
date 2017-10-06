// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"

#include "base/md5.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/ntp/ntp_tile.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ntp_tile_saver {

// Write the |mostVisitedSites| to disk.
void WriteSavedMostVisited(NSDictionary<NSURL*, NTPTile*>* mostVisitedSites);

// Returns the path for the temporary favicon folder.
NSURL* TmpFaviconFolderPath();

// Replaces the current saved favicons at |faviconsURL| by the contents of the
// tmp folder.
void ReplaceSavedFavicons(NSURL* faviconsURL);

// Checks if every site in |tiles| has had its favicons fetched. If so, writes
// the info to disk, saving the favicons to |faviconsURL|.
void WriteToDiskIfComplete(NSDictionary<NSURL*, NTPTile*>* tiles,
                           NSURL* faviconsURL);

// Gets a name for the favicon file.
NSString* GetFaviconFileName(const GURL& url);

// If the sites currently saved include one with |tile|'s url, replace it by
// |tile|.
void WriteSingleUpdatedTileToDisk(NTPTile* tile);

}  // namespace ntp_tile_saver

namespace ntp_tile_saver {

NSURL* TmpFaviconFolderPath() {
  return [[NSURL fileURLWithPath:NSTemporaryDirectory()]
      URLByAppendingPathComponent:@"tmpFaviconFolder"];
}

void ReplaceSavedFavicons(NSURL* faviconsURL) {
  if ([[NSFileManager defaultManager] fileExistsAtPath:[faviconsURL path]]) {
    [[NSFileManager defaultManager] removeItemAtURL:faviconsURL error:nil];
  }

  if ([[NSFileManager defaultManager]
          fileExistsAtPath:[TmpFaviconFolderPath() path]]) {
    [[NSFileManager defaultManager]
               createDirectoryAtURL:faviconsURL.URLByDeletingLastPathComponent
        withIntermediateDirectories:YES
                         attributes:nil
                              error:nil];

    [[NSFileManager defaultManager] moveItemAtURL:TmpFaviconFolderPath()
                                            toURL:faviconsURL
                                            error:nil];
  }
}

void WriteToDiskIfComplete(NSDictionary<NSURL*, NTPTile*>* tiles,
                           NSURL* faviconsURL) {
  for (NSURL* siteURL : tiles) {
    NTPTile* tile = [tiles objectForKey:siteURL];
    if (!tile.faviconFetched) {
      return;
    }
    // Any fetched tile must have a file name or a fallback monogram.
    DCHECK(tile.faviconFileName || tile.fallbackMonogram);
  }

  ReplaceSavedFavicons(faviconsURL);
  WriteSavedMostVisited(tiles);
}

NSString* GetFaviconFileName(const GURL& url) {
  return [base::SysUTF8ToNSString(base::MD5String(url.spec()))
      stringByAppendingString:@".png"];
}

void SaveMostVisitedToDisk(const ntp_tiles::NTPTilesVector& mostVisitedData,
                           id<GoogleLandingDataSource> faviconFetcher,
                           NSURL* faviconsURL) {
  if (faviconsURL == nil) {
    return;
  }

  NSMutableDictionary<NSURL*, NTPTile*>* tiles =
      [[NSMutableDictionary alloc] init];

  // Clear the tmp favicon folder and recreate it.
  NSURL* tmpFaviconURL = TmpFaviconFolderPath();
  if ([[NSFileManager defaultManager] fileExistsAtPath:[tmpFaviconURL path]]) {
    [[NSFileManager defaultManager] removeItemAtURL:tmpFaviconURL error:nil];
  }
  [[NSFileManager defaultManager] createDirectoryAtPath:[tmpFaviconURL path]
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:nil];

  // If there are 0 sites to display, the for loop below will not be entered.
  // Write the updated empty list of sites to disk before returning.
  if (mostVisitedData.size() == 0) {
    WriteToDiskIfComplete(tiles, faviconsURL);
  }

  // For each site, get the favicon. If it is returned, write it to the favicon
  // tmp folder. If a fallback value is returned, update the tile info. Calls
  // WriteToDiskIfComplete after each callback execution.
  // All the sites are added first to the list so that the WriteToDiskIfComplete
  // command is not passed an incomplete list.
  for (size_t i = 0; i < mostVisitedData.size(); i++) {
    const ntp_tiles::NTPTile& ntpTile = mostVisitedData[i];
    NTPTile* tile =
        [[NTPTile alloc] initWithTitle:base::SysUTF16ToNSString(ntpTile.title)
                                   URL:net::NSURLWithGURL(ntpTile.url)
                              position:i];
    [tiles setObject:tile forKey:tile.URL];
  }

  for (NTPTile* tile : [tiles objectEnumerator]) {
    const GURL& gurl = net::GURLWithNSURL(tile.URL);
    NSString* faviconFileName = GetFaviconFileName(gurl);
    NSURL* fileURL =
        [tmpFaviconURL URLByAppendingPathComponent:faviconFileName];

    void (^faviconImageBlock)(UIImage*) = ^(UIImage* favicon) {
      NSData* imageData = UIImagePNGRepresentation(favicon);
      if ([imageData writeToURL:fileURL atomically:YES]) {
        // If saving the image is not possible, the best thing to do is to keep
        // the previous tile.
        tile.faviconFetched = YES;
        tile.faviconFileName = faviconFileName;
        WriteToDiskIfComplete(tiles, faviconsURL);
      }
    };

    void (^fallbackBlock)(UIColor*, UIColor*, BOOL) =
        ^(UIColor* textColor, UIColor* backgroundColor, BOOL isDefaultColor) {
          tile.faviconFetched = YES;
          tile.fallbackTextColor = textColor;
          tile.fallbackBackgroundColor = backgroundColor;
          tile.fallbackIsDefaultColor = isDefaultColor;
          tile.fallbackMonogram = base::SysUTF16ToNSString(
              favicon::GetFallbackIconText(net::GURLWithNSURL(tile.URL)));
          DCHECK(tile.fallbackMonogram && tile.fallbackTextColor &&
                 tile.fallbackBackgroundColor);
          WriteToDiskIfComplete(tiles, faviconsURL);
        };

    // The cache is not used here as it is simply a way to have a synchronous
    // immediate return. In this case it is unnecessary.

    [faviconFetcher getFaviconForURL:gurl
                                size:48
                            useCache:NO
                       imageCallback:faviconImageBlock
                    fallbackCallback:fallbackBlock];
  }
}

void WriteSingleUpdatedTileToDisk(NTPTile* tile) {
  NSMutableDictionary* tiles = [ReadSavedMostVisited() mutableCopy];
  [tiles setObject:tile forKey:tile.URL];
  WriteSavedMostVisited(tiles);
}

void WriteSavedMostVisited(NSDictionary<NSURL*, NTPTile*>* mostVisitedSites) {
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:mostVisitedSites];
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults setObject:data forKey:app_group::kSuggestedItems];

  // TODO(crbug.com/750673): Update the widget's visibility depending on
  // availability of sites.
}

NSDictionary* ReadSavedMostVisited() {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  return [NSKeyedUnarchiver
      unarchiveObjectWithData:[sharedDefaults
                                  objectForKey:app_group::kSuggestedItems]];
}

void UpdateSingleFavicon(const GURL& siteURL,
                         id<GoogleLandingDataSource> faviconFetcher,
                         NSURL* faviconsURL) {
  NSDictionary* tiles = ReadSavedMostVisited();

  NSURL* siteNSURL = net::NSURLWithGURL(siteURL);
  NTPTile* tile = [tiles objectForKey:siteNSURL];
  if (!tile) {
    return;
  }
  // Remove existing favicon info
  tile.fallbackTextColor = nil;
  tile.fallbackBackgroundColor = nil;
  tile.faviconFetched = NO;
  NSString* previousFaviconFileName = tile.faviconFileName;
  tile.faviconFileName = nil;

  // Fetch favicon and update saved defaults.
  NSString* faviconFileName = GetFaviconFileName(siteURL);
  NSURL* fileURL = [faviconsURL URLByAppendingPathComponent:faviconFileName];
  NSURL* previousFileURL =
      previousFaviconFileName
          ? [faviconsURL URLByAppendingPathComponent:previousFaviconFileName]
          : nil;
  NSString* monogram =
      base::SysUTF16ToNSString(favicon::GetFallbackIconText(siteURL));

  void (^faviconImageBlock)(UIImage*) = ^(UIImage* favicon) {
    tile.faviconFetched = YES;
    NSData* imageData = UIImagePNGRepresentation(favicon);
    [[NSFileManager defaultManager] removeItemAtURL:previousFileURL error:nil];
    if ([imageData writeToURL:fileURL atomically:YES]) {
      tile.faviconFileName = faviconFileName;
    }
    WriteSingleUpdatedTileToDisk(tile);
  };

  void (^fallbackBlock)(UIColor*, UIColor*, BOOL) =
      ^(UIColor* textColor, UIColor* backgroundColor, BOOL isDefaultColor) {
        tile.faviconFetched = YES;
        tile.fallbackTextColor = textColor;
        tile.fallbackBackgroundColor = backgroundColor;
        tile.fallbackIsDefaultColor = isDefaultColor;
        tile.fallbackMonogram = monogram;
        [[NSFileManager defaultManager] removeItemAtURL:previousFileURL
                                                  error:nil];
        WriteSingleUpdatedTileToDisk(tile);
      };

  // The cache is not used here as it is simply a way to have a synchronous
  // immediate return. In this case it is unnecessary.
  [faviconFetcher getFaviconForURL:siteURL
                              size:48
                          useCache:NO
                     imageCallback:faviconImageBlock
                  fallbackCallback:fallbackBlock];
}
}  // namespace ntp_tile_saver
