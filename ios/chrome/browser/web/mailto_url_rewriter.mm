// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/mailto_url_rewriter.h"

#import "base/logging.h"
#import "ios/chrome/browser/web/mailto_handler.h"
#import "ios/chrome/browser/web/mailto_handler_gmail.h"
#import "ios/chrome/browser/web/mailto_handler_system_mail.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MailtoURLRewriter
@synthesize observer = _observer;
@dynamic defaultHandlers;

- (NSString*)defaultHandlerID {
  NOTREACHED();
  return nil;
}

- (void)setDefaultHandlerID:(NSString*)defaultHandlerID {
  NOTREACHED();
}

+ (NSString*)userDefaultsKey {
  return nil;
}

+ (NSString*)systemMailApp {
  // This is the App Store ID for Apple Mail app.
  // See https://itunes.apple.com/us/app/mail/id1108187098?mt=8
  return @"1108187098";
}

+ (instancetype)mailtoURLRewriterWithStandardHandlers {
  NOTREACHED();
  return nil;
}

- (void)setDefaultHandlers:(NSArray<MailtoHandler*>*)defaultHandlers {
  NOTREACHED();
}

- (NSArray<MailtoHandler*>*)defaultHandlers {
  NOTREACHED();
  return nil;
}

- (NSString*)defaultHandlerName {
  NOTREACHED();
  return nil;
}

- (MailtoHandler*)defaultHandlerByID:(NSString*)handlerID {
  NOTREACHED();
  return nil;
}

- (NSString*)rewriteMailtoURL:(const GURL&)gURL {
  NOTREACHED();
  return nil;
}

@end
