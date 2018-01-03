// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_H_

#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/public/abstract_web_toolbar.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"

@protocol Toolbar<AbstractWebToolbar,
                  OmniboxFocuser,
                  VoiceSearchControllerDelegate,
                  ActivityServicePositioner,
                  QRScannerResultLoading,
                  BubbleViewAnchorPointProvider,
                  ToolsMenuPresentationProvider,
                  FullscreenUIElement>

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible;
- (void)start;
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_H_
