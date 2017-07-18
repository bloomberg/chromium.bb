// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/contextual_search_wrangler.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_controller.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_mask_view.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_metrics.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#import "ios/chrome/browser/ui/contextual_search/touch_to_search_permissions_mediator.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextualSearchWrangler ()<ContextualSearchControllerDelegate,
                                       ContextualSearchPanelMotionObserver,
                                       TabModelObserver>
@property(nonatomic, readonly, weak) TabModel* tabModel;
@property(nonatomic, readonly, weak) id<ContextualSearchProvider> provider;
@property(nonatomic) ContextualSearchPanelView* panel;
@property(nonatomic) ContextualSearchController* controller;
@property(nonatomic) ContextualSearchMaskView* mask;

@end

@implementation ContextualSearchWrangler

@synthesize tabModel = _tabModel;
@synthesize provider = _provider;
@synthesize panel = _panel;
@synthesize controller = _controller;
@synthesize mask = _mask;

- (instancetype)initWithProvider:(id<ContextualSearchProvider>)provider
                        tabModel:(TabModel*)tabModel {
  if ((self = [super init])) {
    _provider = provider;
    _tabModel = tabModel;
  }
  return self;
}

- (void)enable:(BOOL)enabled {
  [self.controller enableContextualSearch:enabled];
  if (enabled)
    [self.tabModel addObserver:self];
  else
    [self.tabModel removeObserver:self];
}

- (void)insertPanelView {
  // Move panel back into its correct place.
  [self.provider.view insertSubview:self.panel
                       aboveSubview:self.provider.toolbarView];
}

- (void)maybeStartForBrowserState:(ios::ChromeBrowserState*)browserState {
  // Create contextual search views and controller.
  if ([TouchToSearchPermissionsMediator isTouchToSearchAvailableOnDevice] &&
      !browserState->IsOffTheRecord()) {
    self.mask = [[ContextualSearchMaskView alloc] init];
    [self.provider.view insertSubview:self.mask
                         belowSubview:self.provider.toolbarView];
    self.panel = [self createPanelView];
    [self.provider.view insertSubview:self.panel
                         aboveSubview:self.provider.toolbarView];
    self.controller =
        [[ContextualSearchController alloc] initWithBrowserState:browserState
                                                        delegate:self];
    [self.controller setPanel:self.panel];
    [self.controller setTab:[self.tabModel currentTab]];
    [self enable:YES];
  }
}

- (void)stop {
  [self.tabModel removeObserver:self];
  [self.controller close];
  self.controller = nil;
  [self.panel removeFromSuperview];
  [self.mask removeFromSuperview];
}

#pragma mark - ContextualSearchControllerDelegate

- (void)promotePanelToTabProvidedBy:(id<ContextualSearchTabProvider>)tabProvider
                         focusInput:(BOOL)focusInput {
  // Tell the panel it will be promoted.
  ContextualSearchPanelView* promotingPanel = self.panel;
  [promotingPanel prepareForPromotion];

  // Make a new panel and tell the controller about it.
  self.panel = [self createPanelView];
  [self.provider.view insertSubview:self.panel belowSubview:promotingPanel];
  [self.controller setPanel:self.panel];

  // Figure out vertical offset.
  CGFloat offset = StatusBarHeight();
  if (IsIPadIdiom()) {
    offset = MAX(offset, CGRectGetMaxY(self.provider.tabStripView.frame));
  }

  // Transition steps: Animate the panel position, fade in the toolbar and
  // tab strip.
  ProceduralBlock transition = ^{
    [promotingPanel promoteToMatchSuperviewWithVerticalOffset:offset];
    [self.provider updateToolbarControlsAlpha:1.0];
    [self.provider updateToolbarBackgroundAlpha:1.0];
    self.provider.tabStripView.alpha = 1.0;
  };

  // After the transition animation completes, add the tab to the tab model
  // (on iPad this triggers the tab strip animation too), then fade out the
  // transitioning panel and remove it.
  void (^completion)(BOOL) = ^(BOOL finished) {
    self.mask.alpha = 0;
    std::unique_ptr<web::WebState> webState = [tabProvider releaseWebState];
    DCHECK(webState);
    DCHECK(webState->GetNavigationManager());

    Tab* newTab = LegacyTabHelper::GetTabForWebState(webState.get());
    WebStateList* webStateList = [self.tabModel webStateList];

    // Insert the new tab after the current tab.
    DCHECK_NE(webStateList->active_index(), WebStateList::kInvalidIndex);
    DCHECK_NE(webStateList->active_index(), INT_MAX);
    int insertion_index = webStateList->active_index() + 1;
    webStateList->InsertWebState(insertion_index, std::move(webState));
    webStateList->SetOpenerOfWebStateAt(insertion_index,
                                        [tabProvider webStateOpener]);

    // Set isPrerenderTab to NO after inserting the tab. This will allow the
    // BrowserViewController to detect that a pre-rendered tab is switched in,
    // and show the prerendering animation. This needs to happen before the
    // tab is made the current tab.
    // This also enables contextual search (if otherwise applicable) on
    // |newTab|.

    newTab.isPrerenderTab = NO;
    [self.tabModel setCurrentTab:newTab];

    if (newTab.loadFinished)
      [self.provider tabLoadComplete:newTab withSuccess:YES];

    if (focusInput) {
      [self.provider focusOmnibox];
    }
    [self.provider restoreInfobars];

    [UIView animateWithDuration:ios::material::kDuration2
        animations:^{
          promotingPanel.alpha = 0;
        }
        completion:^(BOOL finished) {
          [promotingPanel removeFromSuperview];
        }];
  };

  [UIView animateWithDuration:ios::material::kDuration3
                   animations:transition
                   completion:completion];
}

- (void)createTabFromContextualSearchController:(const GURL&)url {
  Tab* currentTab = [self.tabModel currentTab];
  DCHECK(currentTab);
  NSUInteger index = [self.tabModel indexOfTab:currentTab];
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  // This returns a Tab, but it's ignored..
  [self.tabModel insertTabWithLoadParams:params
                                  opener:nil
                             openedByDOM:NO
                                 atIndex:index + 1
                            inBackground:NO];
}

- (CGFloat)currentHeaderHeight {
  return 20.0;
}

#pragma mark - ContextualSearchPanelMotionObserver

- (void)panel:(ContextualSearchPanelView*)panel
    didMoveWithMotion:(ContextualSearch::PanelMotion)motion {
  // If the header is offset, it's offscreen (or moving offscreen) and the
  // toolbar shouldn't be opacity-adjusted by the contextual search panel.
  if ([self.provider currentHeaderOffset] != 0)
    return;

  CGFloat toolbarAlpha;

  if (motion.state == ContextualSearch::PREVIEWING) {
    // As the panel moves past the previewing position, the toolbar should
    // become more transparent.
    toolbarAlpha = 1 - motion.gradation;
  } else if (motion.state == ContextualSearch::COVERING) {
    // The toolbar should be totally transparent when the panel is covering.
    toolbarAlpha = 0.0;
  } else {
    return;
  }

  // On iPad, the toolbar doesn't go fully transparent, so map |toolbarAlpha|'s
  // [0-1.0] range to [0.5-1.0].
  if (IsIPadIdiom()) {
    toolbarAlpha = 0.5 + (toolbarAlpha * 0.5);
    self.provider.tabStripView.alpha = toolbarAlpha;
  }

  [self.provider updateToolbarControlsAlpha:toolbarAlpha];
  [self.provider updateToolbarBackgroundAlpha:toolbarAlpha];
}

- (void)panel:(ContextualSearchPanelView*)panel
    didChangeToState:(ContextualSearch::PanelState)toState
           fromState:(ContextualSearch::PanelState)fromState {
  if (toState == ContextualSearch::DISMISSED) {
    // Panel has become hidden.
    [self.provider restoreInfobars];
    [self.provider updateToolbarControlsAlpha:1.0];
    [self.provider updateToolbarBackgroundAlpha:1.0];
    self.provider.tabStripView.alpha = 1.0;
  } else if (fromState == ContextualSearch::DISMISSED) {
    // Panel has become visible.
    [self.provider suspendInfobars];
  }
}

- (void)panelWillPromote:(ContextualSearchPanelView*)panel {
  [panel removeMotionObserver:self];
}

#pragma mark - TabModelObsever

- (void)tabModel:(TabModel*)model
    didInsertTab:(Tab*)tab
         atIndex:(NSUInteger)modelIndex
    inForeground:(BOOL)fg {
  if (fg) {
    [self.controller setTab:tab];
  }
}

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  [self.controller setTab:newTab];
}

- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab {
  DCHECK(tab && ([model indexOfTab:tab] != NSNotFound));
  if (tab == [self.tabModel currentTab]) {
    // Disable contextual search when |tab| is a voice search result tab.
    [self.controller enableContextualSearch:!tab.isVoiceSearchResultsTab];
  }
}

- (void)tabModel:(TabModel*)model willRemoveTab:(Tab*)tab {
  if ([model count] == 1) {  // About to remove the last tab.
    [self.controller setTab:nil];
  }
}

#pragma mark - internal

// Creates a new panel view and sets up observers; the panel view's
// configuration is based on the traits of the provider's view.
- (ContextualSearchPanelView*)createPanelView {
  PanelConfiguration* config;
  CGSize panelContainerSize = self.provider.view.bounds.size;
  if (IsIPadIdiom()) {
    config = [PadPanelConfiguration
        configurationForContainerSize:panelContainerSize
                  horizontalSizeClass:self.provider.view.traitCollection
                                          .horizontalSizeClass];
  } else {
    config = [PhonePanelConfiguration
        configurationForContainerSize:panelContainerSize
                  horizontalSizeClass:self.provider.view.traitCollection
                                          .horizontalSizeClass];
  }
  ContextualSearchPanelView* newPanel =
      [[ContextualSearchPanelView alloc] initWithConfiguration:config];
  [newPanel addMotionObserver:self];
  [newPanel addMotionObserver:self.mask];
  return newPanel;
}

@end
