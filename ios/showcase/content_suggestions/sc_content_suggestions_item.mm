// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/content_suggestions/sc_content_suggestions_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCContentSuggestionsItem

@synthesize delegate;
@synthesize attributes;
@synthesize suggestionIdentifier;
@synthesize image;
@synthesize title;
@synthesize subtitle;
@synthesize hasImage;
@synthesize publisher;
@synthesize publishDate;
@synthesize availableOffline;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsCell*)cell {
  [super configureCell:cell];
  [cell.faviconView configureWithAttributes:self.attributes];
  cell.titleLabel.text = self.title;
  [cell setSubtitleText:self.subtitle];
  cell.displayImage = self.hasImage;
  [cell setContentImage:self.image];
  [cell setAdditionalInformationWithPublisherName:self.publisher
                                             date:self.publishDate
                              offlineAvailability:self.availableOffline];
}

@end
