// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error.h"

class GlobalErrorBubbleViewBase;
class Profile;

// Shows preferences reset errors on the wrench menu and exposes a menu item to
// launch a bubble view.
class ProfileResetGlobalError
    : public GlobalError,
      public base::SupportsWeakPtr<ProfileResetGlobalError> {
 public:
  explicit ProfileResetGlobalError(Profile* profile);
  virtual ~ProfileResetGlobalError();

  // Called by the bubble view when it is closed.
  void OnBubbleViewDidClose();

  // Called when the user clicks on the 'Reset' button. The user can choose to
  // send feedback containing the old settings that are now being reset, this is
  // indicated by |send_feedback|.
  void OnBubbleViewResetButtonPressed(bool send_feedback);

  // Called when the user clicks the 'No, thanks' button.
  void OnBubbleViewNoThanksButtonPressed();

  // GlobalError:
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual bool HasShownBubbleView() OVERRIDE;
  virtual void ShowBubbleView(Browser* browser) OVERRIDE;
  virtual GlobalErrorBubbleViewBase* GetBubbleView() OVERRIDE;

 private:
  Profile* profile_;

  // The number of times we have shown the bubble so far. This can be >1 if the
  // user dismissed the bubble, then selected the menu item to show it again.
  int num_times_bubble_view_shown_;

  // The reset bubble, if we're currently showing one.
  GlobalErrorBubbleViewBase* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetGlobalError);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
