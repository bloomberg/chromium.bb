// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/net/connection_type_observer_bridge.h"
#import "ios/chrome/browser/prefs/pref_observer_bridge.h"
#import "ios/chrome/browser/prerender/preload_provider.h"
#import "ios/chrome/browser/tabs/tab_delegate.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "net/url_request/url_fetcher.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@protocol PreloadControllerDelegate;

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// PreloadController owns and manages a Tab that contains a prerendered
// webpage.  This class contains methods to queue and cancel prerendering for a
// given URL as well as a method to return the prerendered Tab.
@interface PreloadController : NSObject<CRWNativeContentProvider,
                                        PrefObserverDelegate,
                                        PreloadProvider,
                                        TabDelegate,
                                        CRConnectionTypeObserverBridge>
// The URL of the currently prerendered Tab.  Empty if there is no prerendered
// Tab.
@property(nonatomic, readonly, assign) GURL prerenderedURL;
@property(nonatomic, weak) id<PreloadControllerDelegate> delegate;

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

// Called when the browser state this object was initialized with is being
// destroyed.
- (void)browserStateDestroyed;

// Returns the currently prerendered WebState, or nil if none exists.  After
// this method is called, the PrerenderController reverts to a non-prerendering
// state.
- (std::unique_ptr<web::WebState>)releasePrerenderContents;

@end

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_
