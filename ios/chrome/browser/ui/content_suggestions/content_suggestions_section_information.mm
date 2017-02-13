// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_section_information.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsSectionInformation

@synthesize emptyCell = _emptyCell;
@synthesize layout = _layout;
@synthesize ID = _ID;
@synthesize title = _title;

- (instancetype)initWithID:(ContentSuggestionsSectionID)ID
                 emptyCell:(CollectionViewItem*)emptyCell
                    layout:(ContentSuggestionsSectionLayout)layout
                     title:(NSString*)title {
  self = [super init];
  if (self) {
    DCHECK(ID < ContentSuggestionsSectionCount);
    _ID = ID;
    _layout = layout;
    _emptyCell = emptyCell;
    _title = [title copy];
  }
  return self;
}

@end
