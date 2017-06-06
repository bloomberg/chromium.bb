// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedItem

@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize attributes = _attributes;
@synthesize title = _title;
@synthesize URL = _URL;
@synthesize delegate = _delegate;
@synthesize image = _image;
@synthesize source = _source;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsMostVisitedCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsMostVisitedCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text = self.title;
  cell.accessibilityLabel = self.title;
  [cell.faviconView configureWithAttributes:self.attributes];
}

- (ntp_tiles::TileVisualType)tileType {
  if (!self.attributes) {
    return ntp_tiles::TileVisualType::NONE;
  } else if (self.attributes.faviconImage) {
    return ntp_tiles::TileVisualType::ICON_REAL;
  }
  return self.attributes.defaultBackgroundColor
             ? ntp_tiles::TileVisualType::ICON_DEFAULT
             : ntp_tiles::TileVisualType::ICON_COLOR;
}

@end
