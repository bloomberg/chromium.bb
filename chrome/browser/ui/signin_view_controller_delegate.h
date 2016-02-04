// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_

#include "chrome/browser/ui/profile_chooser_constants.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class ModalSigninDelegate;
class SigninViewController;

namespace signin_metrics {
enum class AccessPoint;
}

// Abstract base class to the platform-specific managers of the Signin and Sync
// confirmation tab-modal dialogs. This and its platform-specific
// implementations are responsible for actually creating and owning the dialogs,
// as well as managing the navigation inside them.
// Subclasses are responsible for deleting themselves when the window they're
// managing closes.
class SigninViewControllerDelegate : public content::WebContentsDelegate {
 public:
  static SigninViewControllerDelegate* CreateModalSigninDelegate(
      SigninViewController* signin_view_controller,
      profiles::BubbleViewMode mode,
      Browser* browser,
      signin_metrics::AccessPoint access_point);

  static SigninViewControllerDelegate* CreateSyncConfirmationDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser);

  void CloseModalSignin();
  void NavigationButtonClicked(content::WebContents* web_contents);

 protected:
  SigninViewControllerDelegate(SigninViewController* signin_view_controller,
                               content::WebContents* web_contents);
  ~SigninViewControllerDelegate() override;

  // Notifies the SigninViewController that this instance is being deleted.
  void ResetSigninViewControllerDelegate();

  // content::WebContentsDelegate
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;

  // This will be called by this base class when the navigation state of the
  // sign in page allows for backwards navigation. It should display a "back"
  // navigation button to the user.
  virtual void ShowBackArrow() = 0;

  // This will be called by this base class when the navigation state of the
  // sign in page doesn't allow for backwards navigation. It should display a
  // "close" button to the user.
  virtual void ShowCloseButton() = 0;

  // This will be called by this base class when the tab-modal window must be
  // closed. This should close the platform-specific window that is currently
  // showing the sign in flow or the sync confirmation dialog.
  virtual void PerformClose() = 0;

 private:
  bool CanGoBack(content::WebContents* web_ui_web_contents) const;

  SigninViewController* signin_view_controller_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegate);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
