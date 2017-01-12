// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/net/connection_type_observer_bridge.h"
#import "ios/chrome/browser/prefs/pref_observer_bridge.h"
#import "ios/chrome/browser/tabs/tab_delegate.h"
#import "ios/chrome/browser/ui/omnibox/preload_provider.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "net/url_request/url_fetcher.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ios {
class ChromeBrowserState;
}

class PrefetchDelegate;
@protocol PreloadControllerDelegate;
@class Tab;

// ID of the URLFetcher responsible for prefetches.
extern const int kPreloadControllerURLFetcherID;

// PreloadController owns and manages a Tab that contains a prerendered
// webpage.  This class contains methods to queue and cancel prerendering for a
// given URL as well as a method to return the prerendered Tab.
//
// This class also handles queuing and canceling prefetch requests for a given
// URL. The prefetched content is stored in the network layer cache so there is
// no API to expose that content to users of this class.
@interface PreloadController : NSObject<CRWNativeContentProvider,
                                        PrefObserverDelegate,
                                        PreloadProvider,
                                        TabDelegate,
                                        CRConnectionTypeObserverBridge> {
 @private
  ios::ChromeBrowserState* browserState_;  // Weak.

  // The prerendered tab.  Can be nil.
  base::scoped_nsobject<Tab> tab_;

  // The URL that is prerendered in |tab_|.  This can be different from the
  // value returned by |tab_.url|, for example in cases where there was a
  // redirect.
  //
  // When choosing whether or not to use a prerendered Tab,
  // BrowserViewController compares the URL being loaded by the omnibox with the
  // URL of the prerendered Tab.  Comparing against the Tab's currently URL
  // could return false negatives in cases of redirect, hence the need to store
  // the originally prerendered URL.
  GURL prerenderedURL_;

  // The URL that is scheduled to be prerendered, its associated transition and
  // referrer. |scheduledTransition_| and |scheduledReferrer_| are not valid
  // when |scheduledURL_| is empty.
  GURL scheduledURL_;
  ui::PageTransition scheduledTransition_;
  web::Referrer scheduledReferrer_;

  // The most-recently prefetched URL, or nil if there have been no prefetched
  // URLs.
  GURL prefetchedURL_;

  // The URLFetcher and associated delegate used to prefetch URLs. The delegate
  // simply forwards callbacks from URLFetcher back to the PrerenderController.
  std::unique_ptr<PrefetchDelegate> prefetcherDelegate_;
  std::unique_ptr<net::URLFetcher> prefetcher_;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> observerBridge_;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar prefChangeRegistrar_;
  // Observer for the WWAN setting.  Contains a valid object only if the
  // instant setting is set to wifi-only.
  std::unique_ptr<ConnectionTypeObserverBridge> connectionTypeObserverBridge_;

  // Whether or not the preference is enabled.
  BOOL enabled_;
  // Whether or not prerendering is only when on wifi.
  BOOL wifiOnly_;
  // Whether or not the current connection is using WWAN.
  BOOL usingWWAN_;

  // Number of successful prerenders (i.e. the user viewed the prerendered page)
  // during the lifetime of this controller.
  int successfulPrerendersPerSessionCount_;

  id<PreloadControllerDelegate> delegate_;  // weak
}

// The URL of the currently prerendered Tab.  Empty if there is no prerendered
// Tab.
@property(nonatomic, readonly, assign) GURL prerenderedURL;
// The URL of the currently prefetched content.  Empty if there is no prefetched
// content.
@property(nonatomic, readonly, assign) GURL prefetchedURL;
@property(nonatomic, assign) id<PreloadControllerDelegate> delegate;

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

// Called when the browser state this object was initialized with is being
// destroyed.
- (void)browserStateDestroyed;

// Returns the currently prerendered Tab, or nil if none exists.  The caller
// must retain the returned Tab if needed.  After this method is called, the
// PrerenderController reverts to a non-prerendering state.
- (Tab*)releasePrerenderContents;

// Returns true if the content of |url| has been prefetched.
- (BOOL)hasPrefetchedURL:(const GURL&)url;
@end

#endif  // IOS_CHROME_BROWSER_UI_PRELOAD_CONTROLLER_H_
