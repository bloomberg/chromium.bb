// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FORCED_REAUTHENTICATION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FORCED_REAUTHENTICATION_DIALOG_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;
class SigninManager;

// A modal dialog that displays a warning message of the auth failure
// and ask user to sign in again.
class ForcedReauthenticationDialog : public views::DialogDelegateView {
 public:
  ~ForcedReauthenticationDialog() override;

  // Shows a warning dialog for |profile|. If there are no Browser windows
  // associated with |profile|, signs out the profile immediately, otherwise the
  // user can clicks accept to sign in again. Dialog will be closed after
  // |countdown_duration| seconds.
  // Dialog will delete itself after closing.
  static ForcedReauthenticationDialog* ShowDialog(
      Profile* profile,
      SigninManager* signin_manager,
      const base::TimeDelta& countdown_duration);

  // override views::DialogDelegateView
  bool Accept() override;
  bool Cancel() override;
  void WindowClosing() override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;

  // override views::View
  void AddedToWidget() override;

 private:
  // Show the dialog for |browser|. The dialog will delete itself after closing.
  ForcedReauthenticationDialog(Browser* browser,
                               SigninManager* signin_manager,
                               const base::TimeDelta& countdown_duration);

  void OnCountDown();
  base::TimeDelta GetTimeRemaining() const;

  Browser* browser_;
  SigninManager* signin_manager_;

  const base::TimeTicks desired_close_time_;

  // The timer which is used to refresh the dialog title to display the
  // remaining time.
  base::RepeatingTimer refresh_timer_;

  DISALLOW_COPY_AND_ASSIGN(ForcedReauthenticationDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FORCED_REAUTHENTICATION_DIALOG_H_
