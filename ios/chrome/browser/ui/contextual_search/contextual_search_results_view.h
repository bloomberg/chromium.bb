// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_RESULTS_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_RESULTS_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/contextual_search/contextual_search_controller.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"
#import "url/gurl.h"

@class Tab;

@protocol ContextualSearchTabPromoter<NSObject>
// Expand the search results displayed in the tab retrieved by |tabRetriever| to
// a new tab, dismissing the panel. If |headerPressed| is YES, then this is
// happening because of a tap on the header view. Otherwise, this is because of
// navigation inside the search view.
- (void)promoteTabHeaderPressed:(BOOL)headerPressed;
@end

@protocol ContextualSearchPreloadChecker<NSObject>
// YES if it's OK to preload search results.
- (BOOL)canPreloadSearchResults;
@end

@interface ContextualSearchResultsView
    : UIView<ContextualSearchPanelMotionObserver,
             ContextualSearchPanelScrollSynchronizer,
             ContextualSearchTabProvider>
@property(nonatomic, assign) BOOL active;
// The tab that is credited with opening the search results tab.
@property(nonatomic, assign) Tab* opener;
// Object that can handle promoting the search results.
@property(nonatomic, assign) id<ContextualSearchTabPromoter> promoter;
// Object that can determine if search results can be preloaded.
@property(nonatomic, assign) id<ContextualSearchPreloadChecker> preloadChecker;
// YES if the search results have loaded and this view was visible.
@property(nonatomic, readonly) BOOL contentVisible;

// Creates an new (internal) Tab object loading |url|; if |preloadEnabled| is
// YES, then this tab will start loading as soon as possible, otherwise it will
// start loading as soon as this view becomes visible.
- (void)createTabForSearch:(GURL)url preloadEnabled:(BOOL)preloadEnabled;

// Scroll the search results tab content so the top of the loaded page is
// visible.
- (void)scrollToTopAnimated:(BOOL)animated;

// Record metrics for the loaded search results having finished, marking them
// as 'chained' or not according to |chained|.
- (void)recordFinishedSearchChained:(BOOL)chained;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_RESULTS_VIEW_H_
