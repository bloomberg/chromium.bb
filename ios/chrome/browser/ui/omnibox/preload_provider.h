// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_PRELOAD_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_PRELOAD_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {
struct Referrer;
}

// Protocol for an object that provides preloading capabilities.
@protocol PreloadProvider

// Prerenders the given |url| with the given |transition|.  Normally, prerender
// requests are fulfilled after a short delay, to prevent unnecessary prerenders
// while the user is typing.  If |immediately| is YES, this method starts
// prerendering immediately, with no delay.  |immediately| should be set to YES
// only when there is a very high confidence that the user will navigate to the
// given |url|.
//
// If there is already an existing request for |url|, this method does nothing
// and does not reset the delay timer.  If there is an existing request for a
// different URL, this method cancels that request and queues this request
// instead.
- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately;

// Cancels any outstanding prerender requests and destroys any prerendered Tabs.
- (void)cancelPrerender;

// Prefetches the |url|.
- (void)prefetchURL:(const GURL&)url transition:(ui::PageTransition)transition;

// Cancels any outstanding prefetch requests.
- (void)cancelPrefetch;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_PRELOAD_PROVIDER_H_
