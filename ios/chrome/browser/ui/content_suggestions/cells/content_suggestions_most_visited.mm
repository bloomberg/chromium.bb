// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited.h"

#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisited

@synthesize title = _title;
@synthesize attributes = _attributes;

+ (ContentSuggestionsMostVisited*)mostVisitedWithTitle:(NSString*)title
                                            attributes:
                                                (FaviconAttributes*)attributes {
  ContentSuggestionsMostVisited* mostVisited =
      [[ContentSuggestionsMostVisited alloc] init];
  mostVisited.title = title;
  mostVisited.attributes = attributes;
  return mostVisited;
}

@end
