// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadingObserverBridge::UrlLoadingObserverBridge(id<URLLoadingObserver> owner)
    : owner_(owner) {}

void UrlLoadingObserverBridge::TabWillOpenUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabWillOpenURL:transitionType:)]) {
    [owner_ tabWillOpenURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabFailedToOpenUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabFailedToOpenURL:
                                               transitionType:)]) {
    [owner_ tabFailedToOpenURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidPrerenderUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidPrerenderURL:
                                               transitionType:)]) {
    [owner_ tabDidPrerenderURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidReloadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidReloadURL:transitionType:)]) {
    [owner_ tabDidReloadURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidOpenUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidOpenURL:transitionType:)]) {
    [owner_ tabDidOpenURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::NewTabWillOpenUrl(const GURL& url,
                                                 bool in_incognito) {
  if ([owner_ respondsToSelector:@selector(newTabWillOpenURL:inIncognito:)]) {
    [owner_ newTabWillOpenURL:url inIncognito:in_incognito];
  }
}

void UrlLoadingObserverBridge::NewTabDidOpenUrl(const GURL& url,
                                                bool in_incognito) {
  if ([owner_ respondsToSelector:@selector(newTabDidOpenURL:inIncognito:)]) {
    [owner_ newTabDidOpenURL:url inIncognito:in_incognito];
  }
}
