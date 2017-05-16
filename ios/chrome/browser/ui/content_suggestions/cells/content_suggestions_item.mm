// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"

#include "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsItem ()

@property(nonatomic, copy) NSString* subtitle;
// Used to check if the image has already been fetched. There is no way to
// discriminate between failed image download and nonexitent image. The
// suggestion tries to download the image only once.
@property(nonatomic, assign) BOOL imageFetched;

@end

#pragma mark - ContentSuggestionsItem

@implementation ContentSuggestionsItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize image = _image;
@synthesize URL = _URL;
@synthesize publisher = _publisher;
@synthesize publishDate = _publishDate;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize delegate = _delegate;
@synthesize imageFetched = _imageFetched;
@synthesize attributes = _attributes;
@synthesize hasImage = _hasImage;
@synthesize availableOffline = _availableOffline;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                         url:(const GURL&)url {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _URL = url;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsCell*)cell {
  [super configureCell:cell];
  if (self.hasImage && !self.imageFetched) {
    self.imageFetched = YES;
    // Fetch the image. During the fetch the cell's image should still be set.
    [self.delegate loadImageForSuggestedItem:self];
  }
  [cell.faviconView configureWithAttributes:self.attributes];
  cell.titleLabel.text = self.title;
  [cell setSubtitleText:self.subtitle];
  cell.displayImage = self.hasImage;
  [cell setContentImage:self.image];
  NSDate* date =
      [NSDate dateWithTimeIntervalSince1970:self.publishDate.ToDoubleT()];
  [cell setAdditionalInformationWithPublisherName:self.publisher
                                             date:date
                              offlineAvailability:self.availableOffline];
}

@end
