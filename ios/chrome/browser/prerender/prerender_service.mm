// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service.h"

#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/prerender/preload_controller.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PrerenderService::PrerenderService(ios::ChromeBrowserState* browser_state)
    : controller_(
          [[PreloadController alloc] initWithBrowserState:browser_state]) {}

PrerenderService::~PrerenderService() {}

void PrerenderService::Shutdown() {
  [controller_ browserStateDestroyed];
  controller_ = nil;
}

void PrerenderService::SetDelegate(id<PreloadControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

void PrerenderService::StartPrerender(const GURL& url,
                                      const web::Referrer& referrer,
                                      ui::PageTransition transition,
                                      bool immediately) {
  // PrerenderService is not compatible with WKBasedNavigationManager because it
  // loads the URL in a new WKWebView, which doesn't have the current session
  // history. TODO(crbug.com/814789): decide whether PrerenderService needs to
  // be supported after evaluating the performance impact in Finch experiment.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    return;

  [controller_ prerenderURL:url
                   referrer:referrer
                 transition:transition
                immediately:immediately];
}

bool PrerenderService::MaybeLoadPrerenderedURL(const GURL& url,
                                               ui::PageTransition transition,
                                               TabModel* tab_model) {
  if (!HasPrerenderForUrl(url)) {
    CancelPrerender();
    return false;
  }

  std::unique_ptr<web::WebState> new_web_state =
      [controller_ releasePrerenderContents];
  DCHECK(new_web_state);
  WebStateList* web_state_list = tab_model.webStateList;
  DCHECK_NE(WebStateList::kInvalidIndex, web_state_list->active_index());

  web::NavigationManager* active_navigation_manager =
      web_state_list->GetActiveWebState()->GetNavigationManager();
  int lastIndex = active_navigation_manager->GetLastCommittedItemIndex();
  UMA_HISTOGRAM_COUNTS_100("Prerender.PrerenderLoadedOnIndex", lastIndex);

  BOOL onFirstNTP =
      IsVisibleURLNewTabPage(web_state_list->GetActiveWebState()) &&
      lastIndex == 0;
  UMA_HISTOGRAM_BOOLEAN("Prerender.PrerenderLoadedOnFirstNTP", onFirstNTP);

  web::NavigationManager* new_navigation_manager =
      new_web_state->GetNavigationManager();

  if (new_navigation_manager->CanPruneAllButLastCommittedItem()) {
    new_navigation_manager->CopyStateFromAndPrune(active_navigation_manager);
    web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                      std::move(new_web_state));

    // new_web_state is now null after the std::move, so grab a new pointer to
    // it for further updates.
    web::WebState* active_web_state = web_state_list->GetActiveWebState();
    if (!active_web_state->IsLoading()) {
      // If the page has finished loading, take a snapshot.  If the page is
      // still loading, do nothing, as the tab helper will automatically take
      // a snapshot once the load completes.
      SnapshotTabHelper::FromWebState(active_web_state)->UpdateSnapshot();
    }

    bool typed_or_generated_transition =
        PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
        PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED);
    if (typed_or_generated_transition) {
      LoadTimingTabHelper::FromWebState(active_web_state)
          ->DidPromotePrerenderTab();
    }

    [tab_model saveSessionImmediately:NO];
    return true;
  }

  CancelPrerender();
  return false;
}

void PrerenderService::CancelPrerender() {
  [controller_ cancelPrerender];
}

bool PrerenderService::HasPrerenderForUrl(const GURL& url) {
  return url == controller_.prerenderedURL;
}

bool PrerenderService::IsWebStatePrerendered(web::WebState* web_state) {
  return [controller_ isWebStatePrerendered:web_state];
}
