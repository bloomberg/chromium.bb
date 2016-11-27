// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/profile_chooser_constants.h"

class Browser;
class SigninViewControllerDelegate;

namespace signin_metrics {
enum class AccessPoint;
}

// Class responsible for showing and hiding the Signin and Sync Confirmation
// tab-modal dialogs.
class SigninViewController {
 public:
  SigninViewController();
  virtual ~SigninViewController();

  // Returns true if the signin flow should be shown as tab-modal for |mode|.
  static bool ShouldShowModalSigninForMode(profiles::BubbleViewMode mode);

  // Shows the signin flow as a tab modal dialog attached to |browser|'s active
  // web contents.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  void ShowModalSignin(profiles::BubbleViewMode mode,
                       Browser* browser,
                       signin_metrics::AccessPoint access_point);
  void ShowModalSyncConfirmationDialog(Browser* browser);
  void ShowModalSigninErrorDialog(Browser* browser);

  // Closes the tab-modal signin flow previously shown using this
  // SigninViewController, if one exists. Does nothing otherwise.
  void CloseModalSignin();

  // Sets the height of the modal signin dialog.
  void SetModalSigninHeight(int height);

  // Notifies this object that it's |signin_view_controller_delegate_|
  // member has become invalid.
  void ResetModalSigninDelegate();

  SigninViewControllerDelegate* delegate() {
    return signin_view_controller_delegate_;
  }

 private:
  SigninViewControllerDelegate* signin_view_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewController);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
