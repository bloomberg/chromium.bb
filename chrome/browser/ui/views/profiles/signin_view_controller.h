// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/profile_chooser_constants.h"

class Browser;
class ModalSigninDelegate;
class Profile;

namespace content {
class WebContentsDelegate;
}

namespace signin_metrics {
enum class AccessPoint;
}

namespace views {
class WebView;
}

class SigninViewController {
 public:
  SigninViewController();
  ~SigninViewController();

  // Returns true if the signin flow should be shown as tab-modal for |mode|.
  static bool ShouldShowModalSigninForMode(profiles::BubbleViewMode mode);

  // Creates the web view that contains the signin flow in |mode| using
  // |profile| as the web content's profile, then sets |delegate| as the created
  // web content's delegate.
  static views::WebView* CreateGaiaWebView(
      content::WebContentsDelegate* delegate,
      profiles::BubbleViewMode mode,
      Profile* profile,
      signin_metrics::AccessPoint access_point);

  static views::WebView* CreateSyncConfirmationWebView(Profile* profile);

  // Shows the signin flow as a tab modal dialog attached to |browser|'s active
  // web contents.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  void ShowModalSignin(profiles::BubbleViewMode mode,
                       Browser* browser,
                       signin_metrics::AccessPoint access_point);

  void ShowModalSyncConfirmationDialog(Browser* browser);

  // Closes the tab-modal signin flow previously shown using this
  // SigninViewController, if one exists. Does nothing otherwise.
  void CloseModalSignin();

  // Notifies this object that it's |modal_signin_delegate_| member has become
  // invalid.
  void ResetModalSigninDelegate();

 private:
   ModalSigninDelegate* modal_signin_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_H_
