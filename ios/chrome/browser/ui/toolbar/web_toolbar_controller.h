// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/public/abstract_web_toolbar.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"
#include "ios/web/public/navigation_item_list.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class Tab;
@protocol ToolbarCommands;
@protocol UrlLoader;
@protocol WebToolbarDelegate;

namespace ios {
class ChromeBrowserState;
}

// Web-view specific toolbar, adding navigation controls like back/forward,
// omnibox, etc.
@interface WebToolbarController
    : ToolbarController<AbstractWebToolbar,
                        OmniboxFocuser,
                        FakeboxFocuser,
                        QRScannerResultLoading,
                        VoiceSearchControllerDelegate>

// Mark inherited initializer as unavailable.
- (instancetype)initWithStyle:(ToolbarControllerStyle)style
                   dispatcher:(id<BrowserCommands, ToolbarCommands>)dispatcher
    NS_UNAVAILABLE;

// Create a new web toolbar controller whose omnibox is backed by
// |browserState|.
- (instancetype)initWithDelegate:(id<WebToolbarDelegate>)delegate
                       urlLoader:(id<UrlLoader>)urlLoader
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
