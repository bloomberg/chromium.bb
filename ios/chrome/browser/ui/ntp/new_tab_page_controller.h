// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <string>

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/native_content_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

namespace ios {
class ChromeBrowserState;
}

namespace NewTabPage {

enum PanelIdentifier {
  kNone,
  kMostVisitedPanel,
  kBookmarksPanel,
  kOpenTabsPanel,
  kIncognitoPanel,
};

// Converts from a NewTabPage::PanelIdentifier to a URL #fragment
// and vice versa.
PanelIdentifier IdentifierFromFragment(const std::string& fragment);
std::string FragmentFromIdentifier(PanelIdentifier panel);

}  // namespace NewTabPage

@protocol CRWSwipeRecognizerProvider;
@class GoogleLandingController;
@protocol NewTabPagePanelProtocol;
@protocol OmniboxFocuser;
@class TabModel;
@protocol WebToolbarDelegate;
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
// The scoped_nsobjects instance variables |*Controller_| are instances of
// subclasses of NewTabPagePanelProtocol that are created lazily.
// Each Panel is its own controller with the accessible views are added to the
// |newTabPageView_|.
//
// newTabPageView_ (aka |ntpView|) is a horizontally scrollable view that
// contains the *PanelController instances available to the user at the moment.
// A tab-page bar inside |ntpView| provides direct access to the
// *PanelControllers on the scrollable view.
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
                              UIScrollViewDelegate> {
 @private
  base::scoped_nsprotocol<id<NewTabPagePanelProtocol>> bookmarkController_;
  base::scoped_nsobject<GoogleLandingController> googleLandingController_;
  base::scoped_nsprotocol<id<NewTabPagePanelProtocol>> incognitoController_;
  // The currently visible controller, one of the above.
  id<NewTabPagePanelProtocol> currentController_;  // weak
}

@property(nonatomic, assign) id<CRWSwipeRecognizerProvider>
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
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
              tabModel:(TabModel*)tabModel;

// Select a panel based on the given |panelType|.
- (void)selectPanel:(NewTabPage::PanelIdentifier)panelType;

// Returns |YES| if the current visible controller should show the keyboard
// shield.
- (BOOL)wantsKeyboardShield;

// Returns |YES| if the current visible controller allows showing the location
// bar hint text.
- (BOOL)wantsLocationBarHintText;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
