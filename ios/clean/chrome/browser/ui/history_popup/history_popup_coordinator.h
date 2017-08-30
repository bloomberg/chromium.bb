// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_POPUP_HISTORY_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_POPUP_HISTORY_POPUP_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"
#import "ios/web/public/navigation_item_list.h"

@class CommandDispatcher;
@protocol PopupMenuDelegate;
@protocol TabHistoryPresentation;
@protocol TabHistoryPositioner;
@protocol TabHistoryUIUpdater;
@class TabModel;

namespace web {
class WebState;
}

// The coordinator in charge of displaying and dismissing the TabHistoryPopup.
// The TabHistoryPopup is presented when the user long presses the back or
// forward Toolbar button.
@interface HistoryPopupCoordinator : BrowserCoordinator

// The web state this HistoryPopupCoordinator is handling.
@property(nonatomic, assign) web::WebState* webState;
// The navigation items this HistoryPopupCoordinator will present.
@property(nonatomic, assign) web::NavigationItemList navigationItems;
// |positionProvider| provides the presentation origin for the TabHistoryPopup.
@property(nonatomic, weak) id<TabHistoryPositioner> positionProvider;
// |presentationProvider| runs tasks for before and after presenting the
// TabHistoryPopup.
@property(nonatomic, weak) id<TabHistoryPresentation> presentationProvider;
// |tabHistoryUIUpdater| updates the relevant UI before and after presenting
// the TabHistoryPopup.
@property(nonatomic, weak) id<TabHistoryUIUpdater> tabHistoryUIUpdater;
// The ToolbarButtonType that triggered the presentation of this
// HistoryPopupCoordinator.
@property(nonatomic, assign) ToolbarButtonType presentingButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_POPUP_HISTORY_POPUP_COORDINATOR_H_
