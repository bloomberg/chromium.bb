// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_PANEL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_PANEL_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

namespace ios {
class ChromeBrowserState;
}

@protocol UrlLoader;
@protocol WebToolbarDelegate;

@interface IncognitoPanelController : NSObject<NewTabPagePanelProtocol>

// The view that is displaying the incognito* panel.
@property(nonatomic, readonly) UIView* view;

// Init with the given loader object. |loader| may be nil, but isn't
// retained so it must outlive this controller. |browserState| may not be null.
// |webToolbarDelegate| is used to fade the toolbar views on page scroll.
- (id)initWithLoader:(id<UrlLoader>)loader
          browserState:(ios::ChromeBrowserState*)browserState
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_PANEL_CONTROLLER_H_
