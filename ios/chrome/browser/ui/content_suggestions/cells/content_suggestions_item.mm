// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"

#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsItem ()

// Used to check if the image has already been fetched. There is no way to
// discriminate between failed image download and nonexitent image. The
// suggestion tries to download the image only once.
@property(nonatomic, assign) BOOL imageFetched;
// YES if the item has never configured a cell with an image.
@property(nonatomic, assign) BOOL firstTimeWithImage;

@end

#pragma mark - ContentSuggestionsItem

@implementation ContentSuggestionsItem

@synthesize title = _title;
@synthesize image = _image;
@synthesize URL = _URL;
@synthesize publisher = _publisher;
@synthesize publishDate = _publishDate;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize delegate = _delegate;
@synthesize imageFetched = _imageFetched;
@synthesize attributes = _attributes;
@synthesize faviconURL = _faviconURL;
@synthesize hasImage = _hasImage;
@synthesize availableOffline = _availableOffline;
@synthesize firstTimeWithImage = _firstTimeWithImage;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                         url:(const GURL&)url {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsCell class];
    _title = [title copy];
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
  cell.displayImage = self.hasImage;
  [cell setContentImage:self.image animated:self.firstTimeWithImage];
  self.firstTimeWithImage = NO;
  [cell setAdditionalInformationWithPublisherName:self.publisher
                                             date:[self relativeDate]
                              offlineAvailability:self.availableOffline];
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = [self accessibilityLabel];
}

- (void)setImage:(UIImage*)image {
  _image = image;
  if (image)
    self.firstTimeWithImage = YES;
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [self.cellClass heightForWidth:width
                              withImage:self.hasImage
                                  title:self.title
                          publisherName:self.publisher
                        publicationDate:[self relativeDate]
                       availableOffline:self.availableOffline];
}

#pragma mark - Private

// Returns the date of publication relative to now.
- (NSString*)relativeDate {
  int64_t now = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  int64_t elapsed = now - self.publishDate.ToDoubleT();
  NSString* relativeDate;
  if (elapsed < 60) {
    // This will also catch items added in the future. In that case, show the
    // "just now" string.
    relativeDate = l10n_util::GetNSString(IDS_IOS_READING_LIST_JUST_NOW);
  } else {
    relativeDate =
        base::SysUTF16ToNSString(ui::TimeFormat::SimpleWithMonthAndYear(
            ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
            base::TimeDelta::FromSeconds(elapsed), true));
  }

  return relativeDate;
}

// Returns the accessibility label.
- (NSString*)accessibilityLabel {
  NSString* offlineAvailability = @"";
  if (self.availableOffline) {
    offlineAvailability = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_ACCESSIBILITY_AVAILABLE_OFFLINE);
  }

  return l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_ACCESSIBILITY_LABEL_SUGGESTION,
      base::SysNSStringToUTF16(self.title),
      base::SysNSStringToUTF16(self.publisher),
      base::SysNSStringToUTF16([self relativeDate]),
      base::SysNSStringToUTF16(offlineAvailability));
}

@end
