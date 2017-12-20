// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_OVERLAY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_OVERLAY_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol SigninPresenter;
@class SigninPromoViewConfigurator;
@class SigninPromoViewMediator;
@protocol SyncPresenter;

namespace ios {
class ChromeBrowserState;
}

enum class TabSwitcherPanelOverlayType {
  OVERLAY_PANEL_EMPTY,
  OVERLAY_PANEL_USER_SIGNED_OUT,
  OVERLAY_PANEL_USER_SIGNED_IN_SYNC_OFF,
  OVERLAY_PANEL_USER_SIGNED_IN_SYNC_ON_NO_SESSIONS,
  OVERLAY_PANEL_USER_SIGNED_IN_SYNC_IN_PROGRESS,
  OVERLAY_PANEL_USER_NO_OPEN_TABS,
  OVERLAY_PANEL_USER_NO_INCOGNITO_TABS
};

enum class TabSwitcherSignInPanelsType;

TabSwitcherPanelOverlayType PanelOverlayTypeFromSignInPanelsType(
    TabSwitcherSignInPanelsType signInPanelType);

@interface TabSwitcherPanelOverlayView : UIView

@property(nonatomic, assign) TabSwitcherPanelOverlayType overlayType;
@property(nonatomic, readonly, weak) id<SigninPresenter, SyncPresenter>
    presenter;
@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    dispatcher;
@property(nonatomic) SigninPromoViewMediator* signinPromoViewMediator;

- (instancetype)initWithFrame:(CGRect)frame
                 browserState:(ios::ChromeBrowserState*)browserState
                    presenter:(id<SigninPresenter, SyncPresenter>)presenter
                   dispatcher:
                       (id<ApplicationCommands, BrowserCommands>)dispatcher;

// Should be called when the tab switcher was shown.
- (void)wasShown;
// Should be called when the tab switcher was hidden.
- (void)wasHidden;

// Called when the sign-in promo view should be reloaded.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_OVERLAY_VIEW_H_
