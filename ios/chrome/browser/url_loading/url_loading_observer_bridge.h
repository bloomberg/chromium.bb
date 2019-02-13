// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// Objective-C equivalent of the UrlLoadingObserverBridge class.
@protocol URLLoadingObserver <NSObject>
@optional

// The loader will open |URL| in the current tab. Next state will be
// one of: tabFailedToOpenURL, tabDidPrerenderURL,
// tabDidReloadURL or tabDidOpenURL.
// Invoked by UrlLoadingObserverBridge::TabWillOpenUrl.
- (void)tabWillOpenURL:(GURL)URL
        transitionType:(ui::PageTransition)transitionType;

// The loader didn't succeed opening the requested |URL|. Reason
// can, for example be an incognito mismatch or an induced crash.
// It is possible that the url was opened, but in another tab.
// Invoked by UrlLoadingObserverBridge::TabFailedToOpenUrl.
- (void)tabFailedToOpenURL:(GURL)URL
            transitionType:(ui::PageTransition)transitionType;

// The loader replaced the load with a prerendering.
// Invoked by UrlLoadingObserverBridge::TabDidPrerenderUrl.
- (void)tabDidPrerenderURL:(GURL)URL
            transitionType:(ui::PageTransition)transitionType;

// The loader reloaded the |URL| in the current tab.
// Invoked by UrlLoadingObserverBridge::TabDidReloadUrl.
- (void)tabDidReloadURL:(GURL)URL
         transitionType:(ui::PageTransition)transitionType;

// The loader initiated the |url| loading successfully.
// Invoked by UrlLoadingObserverBridge::TabDidOpenUrl.
- (void)tabDidOpenURL:(GURL)URL
       transitionType:(ui::PageTransition)transitionType;

// The loader will open |URL| in a new tab. Next state will be:
// newTabDidOpenURL.
// Invoked by UrlLoadingObserverBridge::NewTabWillOpenUrl.
- (void)newTabWillOpenURL:(GURL)URL inIncognito:(BOOL)inIncognito;

// The loader initiated the |URL| loading in a new tab successfully.
// Invoked by UrlLoadingObserverBridge::NewTabDidOpenUrl.
- (void)newTabDidOpenURL:(GURL)URL inIncognito:(BOOL)inIncognito;

// The loader will switch to an existing tab with |URL| instead of loading it.
// Next state will be: didSwitchToTabWithURL. Invoked by
// UrlLoadingObserverBridge::NewTabWillOpenUrl.
- (void)willSwitchToTabWithURL:(GURL)URL
              newWebStateIndex:(NSInteger)newWebStateIndex;

// The loader switched to an existing tab with |URL|.
// Invoked by UrlLoadingObserverBridge::NewTabDidOpenUrl.
- (void)didSwitchToTabWithURL:(GURL)URL
             newWebStateIndex:(NSInteger)newWebStateIndex;

@end

// Observer used to update listeners of change of state in url loading.
class UrlLoadingObserverBridge {
 public:
  UrlLoadingObserverBridge(id<URLLoadingObserver> owner);

  void TabWillOpenUrl(const GURL& url, ui::PageTransition transition_type);
  void TabFailedToOpenUrl(const GURL& url, ui::PageTransition transition_type);
  void TabDidPrerenderUrl(const GURL& url, ui::PageTransition transition_type);
  void TabDidReloadUrl(const GURL& url, ui::PageTransition transition_type);
  void TabDidOpenUrl(const GURL& url, ui::PageTransition transition_type);

  void NewTabWillOpenUrl(const GURL& url, bool in_incognito);
  void NewTabDidOpenUrl(const GURL& url, bool in_incognito);

  void WillSwitchToTabWithUrl(const GURL& url, int new_web_state_index);
  void DidSwitchToTabWithUrl(const GURL& url, int new_web_state_index);

 private:
  __weak id<URLLoadingObserver> owner_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_OBSERVER_BRIDGE_H_
