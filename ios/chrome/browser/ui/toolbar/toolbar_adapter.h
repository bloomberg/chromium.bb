// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ADAPTER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ADAPTER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#include "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/legacy_toolbar_coordinator.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"

namespace ios {
class ChromeBrowserState;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ToolbarCoordinatorDelegate;
@protocol UrlLoader;
class WebStateList;

// Temporary Adapter so ToolbarCoordinator can work as a <Toolbar>
// for LegacyToolbarCoordinator.
@interface ToolbarAdapter : NSObject<Toolbar>

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                      browserState:(ios::ChromeBrowserState*)browserState
                      webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<UrlLoader> URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ADAPTER_H_
