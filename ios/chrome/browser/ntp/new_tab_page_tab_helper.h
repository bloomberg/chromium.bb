// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_owning.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

class WebStateList;
@protocol NewTabPageTabHelperDelegate;
@protocol NewTabPageControllerDelegate;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol OmniboxFocuser;
@protocol FakeboxFocuser;
@protocol SnackbarCommands;
@protocol UrlLoader;
@class NewTabPageCoordinator;

// NewTabPageTabHelper which manages a single NTP per tab.
class NewTabPageTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<NewTabPageTabHelper> {
 public:
  ~NewTabPageTabHelper() override;

  static void CreateForWebState(
      web::WebState* web_state,
      WebStateList* web_state_list,
      id<NewTabPageTabHelperDelegate> delegate,
      id<UrlLoader> loader,
      id<NewTabPageControllerDelegate> toolbar_delegate,
      id<ApplicationCommands,
         BrowserCommands,
         OmniboxFocuser,
         FakeboxFocuser,
         SnackbarCommands,
         UrlLoader> dispatcher);

  // Returns true when the current web_state is an NTP and the underlying
  // controllers have been created.
  bool IsActive() const;

  // Returns the current NTP controller.
  id<NewTabPageOwning> GetController() const;

  // Returns the UIViewController for the current NTP.
  // TODO(crbug.com/826369): Currently there's a 1:1 relationship between the
  // webState and the NTP, so we can't delegate this coordinator to the BVC.
  // Consider sharing one NTP per BVC, and removing this ownership.
  UIViewController* GetViewController() const;

  // Instructs the current NTP to dismiss any context menus.
  void DismissModals() const;

 private:
  NewTabPageTabHelper(web::WebState* web_state,
                      WebStateList* web_state_list,
                      id<NewTabPageTabHelperDelegate> delegate,
                      id<UrlLoader> loader,
                      id<NewTabPageControllerDelegate> toolbar_delegate,
                      id<ApplicationCommands,
                         BrowserCommands,
                         OmniboxFocuser,
                         FakeboxFocuser,
                         SnackbarCommands,
                         UrlLoader> dispatcher);

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;

  // The Objective-C NTP coordinator instance.
  // TODO(crbug.com/826369): Currently there's a 1:1 relationship between the
  // webState and the NTP, so we can't delegate this coordinator to the BVC.
  // Consider sharing one NTP per BVC, and moving this to a delegate.
  __strong NewTabPageCoordinator* ntp_coordinator_ = nil;

  // Used to present and dismiss the NTP.
  __weak id<NewTabPageTabHelperDelegate> delegate_ = nil;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageTabHelper);
};

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_
