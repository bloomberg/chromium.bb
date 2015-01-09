// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/global_error/global_error.h"

class AutomaticProfileResetter;
class GlobalErrorBubbleViewBase;
class Profile;

// Encapsulates UI-related functionality for the one-time profile settings reset
// prompt. The UI consists of two parts: (1.) the profile reset (pop-up) bubble,
// and (2.) a menu item in the wrench menu (provided by us being a GlobalError).
class ProfileResetGlobalError
    : public GlobalError,
      public base::SupportsWeakPtr<ProfileResetGlobalError> {
 public:
  explicit ProfileResetGlobalError(Profile* profile);
  ~ProfileResetGlobalError() override;

  // Returns whether or not the reset prompt is supported on this platform.
  static bool IsSupportedOnPlatform(Browser* browser);

  // Called by the bubble view when it is closed.
  void OnBubbleViewDidClose();

  // Called when the user clicks on the 'Reset' button. The user can choose to
  // send feedback containing the old settings that are now being reset, this is
  // indicated by |send_feedback|.
  void OnBubbleViewResetButtonPressed(bool send_feedback);

  // Called when the user clicks the 'No, thanks' button.
  void OnBubbleViewNoThanksButtonPressed();

  // GlobalError:
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;
  GlobalErrorBubbleViewBase* GetBubbleView() override;

 private:
  Profile* profile_;

  // GlobalErrorService owns us, on which AutomaticProfileResetter depends, so
  // during shutdown, it may get destroyed before we are.
  // Note: the AutomaticProfileResetter expects call-backs from us to always be
  // synchronous, so that there will be no call-backs once we are destroyed.
  base::WeakPtr<AutomaticProfileResetter> automatic_profile_resetter_;

  // Used to measure the delay before the bubble actually gets shown.
  base::ElapsedTimer timer_;

  // Whether or not we have already shown the bubble.
  bool has_shown_bubble_view_;

  // The reset bubble, if we're currently showing one.
  GlobalErrorBubbleViewBase* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetGlobalError);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
