// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/ntp/ntp_tile.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class NTPTileSaverControllerTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[testFaviconDirectory() path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:testFaviconDirectory()
                                                error:nil];
    }
  }

  void TearDown() override {
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[testFaviconDirectory() path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:testFaviconDirectory()
                                                error:nil];
    }
    BlockCleanupTest::TearDown();
  }

  NSURL* testFaviconDirectory() {
    return
        [[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                 inDomains:NSUserDomainMask]
            firstObject] URLByAppendingPathComponent:@"testFaviconFolder"];
  }

  void setupMockCallback(id mock,
                         std::set<GURL> imageURLs,
                         std::set<GURL> fallbackURLs) {
    OCMStub([[mock ignoringNonObjectArgs]
                getFaviconForPageURL:GURL()
                                size:0
                            useCache:NO
                            callback:[OCMArg isNotNil]])
        .andDo(^(NSInvocation* invocation) {
          GURL* urltest;
          [invocation getArgument:&urltest atIndex:2];
          if (imageURLs.find(GURL(*urltest)) != imageURLs.end()) {
            __unsafe_unretained void (^callback)(id);
            [invocation getArgument:&callback atIndex:5];
            UIGraphicsBeginImageContext(CGSizeMake(10, 10));
            UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
            UIGraphicsEndImageContext();
            callback([FaviconAttributes attributesWithImage:image]);
          } else if (fallbackURLs.find(GURL(*urltest)) != fallbackURLs.end()) {
            __unsafe_unretained void (^callback)(id);
            [invocation getArgument:&callback atIndex:5];
            callback([FaviconAttributes
                attributesWithMonogram:@"C"
                             textColor:UIColor.whiteColor
                       backgroundColor:UIColor.blueColor
                defaultBackgroundColor:NO]);
          }
        });
  }

  void verifyWithImage(NTPTile* tile,
                       NSString* expectedTitle,
                       NSURL* expectedURL) {
    EXPECT_NSNE(tile, nil);
    EXPECT_NSEQ(tile.title, expectedTitle);
    EXPECT_NSEQ(tile.URL, expectedURL);
    EXPECT_TRUE(tile.faviconFetched);
    EXPECT_NSNE(tile.faviconFileName, nil);
    EXPECT_NSEQ(tile.fallbackTextColor, nil);
    EXPECT_NSEQ(tile.fallbackBackgroundColor, nil);
    EXPECT_TRUE([[NSFileManager defaultManager]
        fileExistsAtPath:[[testFaviconDirectory()
                             URLByAppendingPathComponent:tile.faviconFileName]
                             path]]);
  }

  void verifyWithFallback(NTPTile* tile,
                          NSString* expectedTitle,
                          NSURL* expectedURL) {
    EXPECT_NSNE(tile, nil);
    EXPECT_NSEQ(tile.title, expectedTitle);
    EXPECT_NSEQ(tile.URL, expectedURL);
    EXPECT_TRUE(tile.faviconFetched);
    EXPECT_NSEQ(tile.faviconFileName, nil);
    EXPECT_NSEQ(tile.fallbackTextColor, UIColor.whiteColor);
    EXPECT_NSEQ(tile.fallbackBackgroundColor, UIColor.blueColor);
    EXPECT_EQ(tile.fallbackIsDefaultColor, NO);
  }
};

TEST_F(NTPTileSaverControllerTest, SaveMostVisitedToDisk) {
  ntp_tiles::NTPTile imageTile = ntp_tiles::NTPTile();
  imageTile.title = base::ASCIIToUTF16("Title");
  imageTile.url = GURL("http://image.com");

  ntp_tiles::NTPTile fallbackTile = ntp_tiles::NTPTile();
  fallbackTile.title = base::ASCIIToUTF16("Title");
  fallbackTile.url = GURL("http://fallback.com");

  id mockFaviconFetcher = OCMProtocolMock(@protocol(GoogleLandingDataSource));
  setupMockCallback(mockFaviconFetcher, {imageTile.url}, {fallbackTile.url});

  ntp_tiles::NTPTilesVector tiles = {
      fallbackTile,  // NTP tile with fallback data
      imageTile,     // NTP tile with favicon
  };

  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mockFaviconFetcher,
                                        testFaviconDirectory());

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* savedTiles =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(savedTiles.count, 2U);

  NSString* fallbackTitle = base::SysUTF16ToNSString(fallbackTile.title);
  NSURL* fallbackURL = net::NSURLWithGURL(fallbackTile.url);
  NTPTile* fallbackSavedTile = [savedTiles objectForKey:fallbackURL];
  verifyWithFallback(fallbackSavedTile, fallbackTitle, fallbackURL);

  NSString* imageTitle = base::SysUTF16ToNSString(imageTile.title);
  NSURL* imageURL = net::NSURLWithGURL(imageTile.url);
  NTPTile* imageSavedTile = [savedTiles objectForKey:imageURL];
  verifyWithImage(imageSavedTile, imageTitle, imageURL);
}

TEST_F(NTPTileSaverControllerTest, UpdateSingleFaviconFallback) {
  // Set up test with 3 saved sites, 2 of which have a favicon.
  ntp_tiles::NTPTile imageTile1 = ntp_tiles::NTPTile();
  imageTile1.title = base::ASCIIToUTF16("Title1");
  imageTile1.url = GURL("http://image1.com");

  ntp_tiles::NTPTile imageTile2 = ntp_tiles::NTPTile();
  imageTile2.title = base::ASCIIToUTF16("Title2");
  imageTile2.url = GURL("http://image2.com");

  ntp_tiles::NTPTile fallbackTile = ntp_tiles::NTPTile();
  fallbackTile.title = base::ASCIIToUTF16("Title");
  fallbackTile.url = GURL("http://fallback.com");

  id mockFaviconFetcher = OCMProtocolMock(@protocol(GoogleLandingDataSource));
  setupMockCallback(mockFaviconFetcher, {imageTile1.url, imageTile2.url},
                    {fallbackTile.url});

  ntp_tiles::NTPTilesVector tiles = {imageTile1, fallbackTile, imageTile2};

  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mockFaviconFetcher,
                                        testFaviconDirectory());

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* savedTiles =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(savedTiles.count, 3U);

  NSString* image1Title = base::SysUTF16ToNSString(imageTile1.title);
  NSURL* image1URL = net::NSURLWithGURL(imageTile1.url);
  NTPTile* image1SavedTile = [savedTiles objectForKey:image1URL];
  verifyWithImage(image1SavedTile, image1Title, image1URL);

  NSString* image2Title = base::SysUTF16ToNSString(imageTile2.title);
  NSURL* image2URL = net::NSURLWithGURL(imageTile2.url);
  NTPTile* image2SavedTile = [savedTiles objectForKey:image2URL];
  verifyWithImage(image2SavedTile, image2Title, image2URL);

  NSString* fallbackTitle = base::SysUTF16ToNSString(fallbackTile.title);
  NSURL* fallbackURL = net::NSURLWithGURL(fallbackTile.url);
  NTPTile* fallbackSavedTile = [savedTiles objectForKey:fallbackURL];
  verifyWithFallback(fallbackSavedTile, fallbackTitle, fallbackURL);

  // Mock returning a fallback value for the first image tile.
  id mockFaviconFetcher2 = OCMProtocolMock(@protocol(GoogleLandingDataSource));
  setupMockCallback(mockFaviconFetcher2, {imageTile2.url},
                    {imageTile1.url, fallbackTile.url});
  ntp_tile_saver::UpdateSingleFavicon(imageTile1.url, mockFaviconFetcher2,
                                      testFaviconDirectory());

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* savedTilesAfterUpdate =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(savedTilesAfterUpdate.count, 3U);

  // Verify that the first image tile now has callback data.
  image1SavedTile = [savedTilesAfterUpdate objectForKey:image1URL];
  verifyWithFallback(image1SavedTile, image1Title, image1URL);

  // Verify that the other two tiles did not change.
  image2SavedTile = [savedTiles objectForKey:image2URL];
  verifyWithImage(image2SavedTile, image2Title, image2URL);
  fallbackSavedTile = [savedTilesAfterUpdate objectForKey:fallbackURL];
  verifyWithFallback(fallbackSavedTile, fallbackTitle, fallbackURL);
}

}  // anonymous namespace
