// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeAutocompleteSuggestion

- (instancetype)init {
  self = [super init];
  if (self) {
    _isURL = YES;
    _text = [[NSAttributedString alloc] initWithString:@""];
    _detailText = [[NSAttributedString alloc] initWithString:@""];
    _numberOfLines = 1;
    _suggestionTypeIcon =
        [[UIImage imageNamed:@"omnibox_completion_default_favicon"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _imageURL = GURL();
  }
  return self;
}

- (BOOL)hasImage {
  return self.imageURL.is_valid();
}

@end
