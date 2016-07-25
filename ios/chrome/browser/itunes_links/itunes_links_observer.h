// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_OBSERVER_H_
#define IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/web_state_observer_bridge.h"

namespace web {
class WebState;
};

@protocol StoreKitLauncher;

// Instances of this class observe navigation events. If a page concerning a
// product on the iTunes App Store is loaded, this class will trigger the
// opening of a SKStoreProductViewController presenting the information of the
// said product.
// The goal of this class is to workaround a bug where Apple serves the wrong
// content for itunes.apple.com pages, see http://crbug.com/623016.
@interface ITunesLinksObserver : NSObject<CRWWebStateObserver>
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the object responsible for showing the App Store product page.
// |storeKitLauncher| can be nil.
- (void)setStoreKitLauncher:(id<StoreKitLauncher>)storeKitLauncher;

@end

#endif  // IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_OBSERVER_H_
