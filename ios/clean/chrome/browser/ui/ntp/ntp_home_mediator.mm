// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_mediator.h"

#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeMediator ()
@end

@implementation NTPHomeMediator
@synthesize dispatcher = _dispatcher;

#pragma mark - UrlLoader

- (void)loadURL:(const GURL&)url
             referrer:(const web::Referrer&)referrer
           transition:(ui::PageTransition)transition
    rendererInitiated:(BOOL)rendererInitiated {
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = transition;
  [self.dispatcher loadURL:params];
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
               inIncognito:(BOOL)inIncognito
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
}

- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab {
}

- (void)loadJavaScriptFromLocationBar:(NSString*)script {
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
}

- (void)cancelOmniboxEdit {
}

- (void)focusFakebox {
}

- (void)onFakeboxBlur {
}

- (void)onFakeboxAnimationComplete {
}

@end
