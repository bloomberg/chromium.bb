// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_links/itunes_links_observer.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/storekit_launcher.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ITunesLinksObserver ()

// If |URL| points to a product on itunes.apple.com, returns the product ID.
// Otherwise, returns nil.
// Examples of URLs pointing to products on itunes.apple.com can be found in
// itunes_links_observer_unittest.mm.
+ (NSString*)productIDFromURL:(const GURL&)URL;

@end

@implementation ITunesLinksObserver {
  __weak id<StoreKitLauncher> _storeKitLauncher;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

+ (NSString*)productIDFromURL:(const GURL&)URL {
  if (!URL.SchemeIsHTTPOrHTTPS() || !URL.DomainIs("itunes.apple.com"))
    return nil;
  std::string fileName = URL.ExtractFileName();
  // The first 2 characters must be "id", followed by the app ID.
  if (fileName.length() < 3 || fileName.substr(0, 2) != "id")
    return nil;
  std::string productID = fileName.substr(2);
  return base::SysUTF8ToNSString(productID);
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  GURL URL = webState->GetLastCommittedURL();
  NSString* productID = [ITunesLinksObserver productIDFromURL:URL];
  if (productID)
    [_storeKitLauncher openAppStore:productID];
}

- (void)setStoreKitLauncher:(id<StoreKitLauncher>)storeKitLauncher {
  _storeKitLauncher = storeKitLauncher;
}

@end
