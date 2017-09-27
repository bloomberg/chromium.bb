// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <string>

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/native_content_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

namespace ios {
class ChromeBrowserState;
}

namespace NewTabPage {

// Converts from a NewTabPage::PanelIdentifier to a URL #fragment
// and vice versa.
ntp_home::PanelIdentifier IdentifierFromFragment(const std::string& fragment);
std::string FragmentFromIdentifier(ntp_home::PanelIdentifier panel);

}  // namespace NewTabPage

@protocol ApplicationCommands;
@class BookmarkHomeTabletNTPController;
@protocol BrowserCommands;
@protocol CRWSwipeRecognizerProvider;
@class GoogleLandingViewController;
@protocol IncognitoViewControllerDelegate;
@protocol NewTabPagePanelProtocol;
@protocol OmniboxFocuser;
@class TabModel;
@protocol UrlLoader;

// This protocol provides callbacks for when the NewTabPageController changes
// panels.
@protocol NewTabPageControllerObserver
// The current visible panel has changed.
- (void)selectedPanelDidChange;
@end

// A controller for the New Tab Page user interface. Supports multiple "panels",
// each with its own controller. The panels are created lazily.
//
// The strongly retained instance variables |*Controller_| are instances of
// subclasses of NewTabPagePanelProtocol that are created lazily.
// Each Panel is its own controller with the accessible views are added to the
// |ntpView_|.
//
// newTabPageView_ is a horizontally scrollable view that contains the
// *PanelController instances available to the user at the moment. A tab-page
// bar inside |ntpView| provides direct access to the *PanelControllers on the
// scrollable view.
//
// The currently visible *PanelController is accessible through
// |currentController_|.
//
@interface NewTabPageController
    : NativeContentController<LogoAnimationControllerOwnerOwner,
                              NewTabPageBarDelegate,
                              NewTabPagePanelControllerDelegate,
                              ToolbarOwner,
                              UIGestureRecognizerDelegate,
                              UIScrollViewDelegate>

@property(nonatomic, weak) id<CRWSwipeRecognizerProvider>
    swipeRecognizerProvider;

// Init with the given url (presumably "chrome://newtab") and loader object.
// |loader| may be nil, but isn't retained so it must outlive this controller.
// Dominant color cache is passed to bookmark controller only, to optimize
// favicon processing.
- (id)initWithUrl:(const GURL&)url
                  loader:(id<UrlLoader>)loader
                 focuser:(id<OmniboxFocuser>)focuser
             ntpObserver:(id<NewTabPageControllerObserver>)ntpObserver
            browserState:(ios::ChromeBrowserState*)browserState
              colorCache:(NSMutableDictionary*)colorCache
         toolbarDelegate:(id<IncognitoViewControllerDelegate>)toolbarDelegate
                tabModel:(TabModel*)tabModel
    parentViewController:(UIViewController*)parentViewController
              dispatcher:(id<ApplicationCommands,
                             BrowserCommands,
                             OmniboxFocuser,
                             UrlLoader>)dispatcher;

// Select a panel based on the given |panelType|.
- (void)selectPanel:(ntp_home::PanelIdentifier)panelType;

// Returns |YES| if the current visible controller should show the keyboard
// shield.
- (BOOL)wantsKeyboardShield;

// Returns |YES| if the current visible controller allows showing the location
// bar hint text.
- (BOOL)wantsLocationBarHintText;

@end

#pragma mark - Testing

@class NewTabPageView;

@interface NewTabPageController (TestSupport)
- (id<NewTabPagePanelProtocol>)currentController;
- (BookmarkHomeTabletNTPController*)bookmarkController;
- (GoogleLandingViewController*)googleLandingController;
- (id<NewTabPagePanelProtocol>)incognitoController;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
