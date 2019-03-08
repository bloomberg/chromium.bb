// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeAutocompleteSuggestion

- (BOOL)supportsDeletion {
  return NO;
}

- (BOOL)hasAnswer {
  return NO;
}

- (BOOL)isURL {
  return YES;
}

- (BOOL)isAppendable {
  return YES;
}

- (int)imageID {
  return 0;
}

- (UIImage*)suggestionTypeIcon {
  return nil;
}

- (BOOL)isTabMatch {
  return NO;
}

- (NSAttributedString*)text {
  return [[NSAttributedString alloc] initWithString:@"Test"];
}

- (NSAttributedString*)detailText {
  return [[NSAttributedString alloc] initWithString:@"Test 2"];
}

- (NSInteger)numberOfLines {
  return 1;
}

- (BOOL)hasImage {
  return NO;
}

- (GURL)imageURL {
  return GURL();
}

@end
