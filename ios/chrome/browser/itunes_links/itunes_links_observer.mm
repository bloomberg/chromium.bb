// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_links/itunes_links_observer.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
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
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge used to observe C++ WebState instance.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
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

#pragma mark - WebStateObserverBridge

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  GURL URL = webState->GetLastCommittedURL();
  NSString* productID = [ITunesLinksObserver productIDFromURL:URL];
  if (productID) {
    StoreKitTabHelper* tabHelper = StoreKitTabHelper::FromWebState(_webState);
    if (tabHelper)
      tabHelper->OpenAppStore(productID);
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState = nullptr;
}

#pragma mark - Public methods.

- (void)setStoreKitLauncher:(id<StoreKitLauncher>)storeKitLauncher {
  StoreKitTabHelper* tabHelper = StoreKitTabHelper::FromWebState(_webState);
  if (tabHelper)
    tabHelper->SetLauncher(storeKitLauncher);
}

@end
