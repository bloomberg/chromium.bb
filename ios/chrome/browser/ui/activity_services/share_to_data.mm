// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/activity_services/share_to_data.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/tabs/tab.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShareToData () {
 @private
  // URL to be shared.
  GURL url_;

  // Title to be shared (not nil).
  NSString* title_;

  // Whether the title was provided by the page (i.e., was not generated from
  // the url).
  BOOL isOriginalTitle_;

  // Whether the page is printable or not.
  BOOL isPagePrintable_;
}

@property(nonatomic, readwrite, copy) NSString* title;
@property(nonatomic, readwrite, assign) BOOL isOriginalTitle;
@property(nonatomic, readwrite, assign) BOOL isPagePrintable;
@end

@implementation ShareToData

@synthesize title = title_;
@synthesize image = image_;
@synthesize isOriginalTitle = isOriginalTitle_;
@synthesize isPagePrintable = isPagePrintable_;

- (id)init {
  NOTREACHED();
  return nil;
}

- (id)initWithURL:(const GURL&)url
              title:(NSString*)title
    isOriginalTitle:(BOOL)isOriginalTitle
    isPagePrintable:(BOOL)isPagePrintable {
  DCHECK(url.is_valid());
  DCHECK(title);
  self = [super init];
  if (self) {
    url_ = url;
    self.title = title;
    self.isOriginalTitle = isOriginalTitle;
    self.isPagePrintable = isPagePrintable;
  }
  return self;
}

- (const GURL&)url {
  return url_;
}

- (NSURL*)nsurl {
  return net::NSURLWithGURL(url_);
}

- (BOOL)isEqual:(id)object {
  if (![object isMemberOfClass:self.class])
    return NO;
  DCHECK(self.url.is_valid());
  DCHECK(self.title);
  ShareToData* other = (ShareToData*)object;
  return self.url == other.url && [self.title isEqual:other.title] &&
         self.image == other.image &&
         self.isOriginalTitle == other.isOriginalTitle;
}

- (NSUInteger)hash {
  DCHECK(self.url.is_valid());
  DCHECK(self.title);
  const NSUInteger kPrime = 31;
  NSString* urlString = base::SysUTF8ToNSString(self.url.spec());
  return kPrime * kPrime * kPrime * urlString.hash +
         kPrime * kPrime * self.title.hash + kPrime * self.image.hash +
         (self.isOriginalTitle ? 0 : 1);
}

@end
