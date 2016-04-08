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
  // Creates and displays a constrained window off of |host_web_contents|,
  // containing |web_contents|. If |wait_for_size| is true, the delegate will
  // wait for ResizeNativeView() to be called by the base class before
  // displaying the constrained window. Otherwise, the window's dimensions will
  // be |frame|.
  SigninViewControllerDelegateMac(
      SigninViewController* signin_view_controller,
      std::unique_ptr<content::WebContents> web_contents,
      content::WebContents* host_web_contents,
      NSRect frame,
      bool wait_for_size);

  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  // Creates the web view that contains the signin flow in |mode| using
  // |profile| as the web content's profile, then sets |delegate| as the created
  // web content's delegate.
  static std::unique_ptr<content::WebContents> CreateGaiaWebContents(
      content::WebContentsDelegate* delegate,
      profiles::BubbleViewMode mode,
      Profile* profile,
      signin_metrics::AccessPoint access_point);

  static std::unique_ptr<content::WebContents>
  CreateSyncConfirmationWebContents(Profile* profile);

 private:
  void PerformClose() override;
  void ResizeNativeView(int height) override;

  void DisplayModal();

  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  ~SigninViewControllerDelegateMac() override;

  // The constrained window opened by this delegate to display signin flow
  // content.
  std::unique_ptr<ConstrainedWindowMac> constrained_window_;

  // The web contents displayed in the constrained window.
  std::unique_ptr<content::WebContents> web_contents_;
  base::scoped_nsobject<ConstrainedWindowCustomWindow> window_;

  // wait_for_size_ stores whether the dialog should only be shown after its
  // content's size has been laid out and measured so that the constrained
  // window is sized to the content.
  bool wait_for_size_;

  // The web contents that the constrained window is displayed off of.
  // Typically, this is going to be the active tab when the window is shown.
  content::WebContents* host_web_contents_;
  NSRect window_frame_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegateMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_MAC_H_
