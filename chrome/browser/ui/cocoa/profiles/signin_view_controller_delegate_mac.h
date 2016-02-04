// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"

@class ConstrainedWindowCustomWindow;
class ConstrainedWindowMac;
@class NavigationButtonClickedHandler;
class Profile;

namespace content {
class WebContents;
class WebContentsDelegate;
}

namespace signin_metrics {
enum class AccessPoint;
}

// Cocoa implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they manage is
// closed (in the OnConstrainedWindowClosed callback).
class SigninViewControllerDelegateMac : public ConstrainedWindowMacDelegate,
                                        public SigninViewControllerDelegate {
 public:
  SigninViewControllerDelegateMac(SigninViewController* signin_view_controller,
                                  scoped_ptr<content::WebContents> web_contents,
                                  content::WebContents* host_web_contents,
                                  NSRect frame);

  static
  SigninViewControllerDelegateMac* CreateModalSigninDelegateWithNavigation(
      SigninViewController* signin_view_controller,
      scoped_ptr<content::WebContents> web_contents,
      content::WebContents* host_web_contents,
      NSRect frame);

  void ButtonClicked();
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  // Creates the web view that contains the signin flow in |mode| using
  // |profile| as the web content's profile, then sets |delegate| as the created
  // web content's delegate.
  static scoped_ptr<content::WebContents> CreateGaiaWebContents(
      content::WebContentsDelegate* delegate,
      profiles::BubbleViewMode mode,
      Profile* profile,
      signin_metrics::AccessPoint access_point);

  static scoped_ptr<content::WebContents> CreateSyncConfirmationWebContents(
      Profile* profile);

 private:
  void ShowBackArrow() override;
  void ShowCloseButton() override;
  void PerformClose() override;

  ~SigninViewControllerDelegateMac() override;
  void AddNavigationButton();

  base::scoped_nsobject<NSButton> back_button_;
  base::scoped_nsobject<NSView> host_view_;
  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  scoped_ptr<content::WebContents> web_contents_;
  base::scoped_nsobject<ConstrainedWindowCustomWindow> window_;
  base::scoped_nsobject<NavigationButtonClickedHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegateMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_MAC_H_
