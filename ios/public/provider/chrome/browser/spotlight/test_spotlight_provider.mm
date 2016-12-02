// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/spotlight/test_spotlight_provider.h"

bool TestSpotlightProvider::IsSpotlightEnabled() {
  return true;
}

NSString* TestSpotlightProvider::GetBookmarkDomain() {
  return @"org.chromium.bookmarks";
}

NSString* TestSpotlightProvider::GetTopSitesDomain() {
  return @"org.chromium.topsites";
}

NSString* TestSpotlightProvider::GetActionsDomain() {
  return @"org.chromium.actions";
}

NSString* TestSpotlightProvider::GetCustomAttributeItemID() {
  return @"OrgChromiumItemID";
}

NSArray* TestSpotlightProvider::GetAdditionalKeywords() {
  return @[ @"chromium" ];
}
