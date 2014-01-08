// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_CONFIRMATION_DIALOG_VIEW_H_
#define ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_CONFIRMATION_DIALOG_VIEW_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

namespace ash {
namespace internal {

class LogoutButtonTray;

// This class implements dialog view for logout confirmation.
class ASH_EXPORT LogoutConfirmationDialogView
    : public views::DialogDelegateView {
 public:
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    virtual void LogoutCurrentUser() = 0;
    virtual base::TimeTicks GetCurrentTime() const = 0;
    virtual void ShowDialog(views::DialogDelegate* dialog) = 0;
  };

  LogoutConfirmationDialogView(LogoutButtonTray* owner, Delegate* delegate);
  virtual ~LogoutConfirmationDialogView();

  // views::DialogDelegateView:
  virtual bool Accept() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual base::string16
      GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  void Show(base::TimeDelta duration);

  // The duration of the confirmation dialog can be changed while the dialog
  // is still showing. This method is only expected to be called when the
  // preference for confirmation dialog duration has changed.
  void UpdateDialogDuration(base::TimeDelta duration);

 private:
  void LogoutCurrentUser();
  void UpdateCountdown();

  views::Label* text_label_;

  base::TimeTicks countdown_start_time_;
  base::TimeDelta duration_;

  base::RepeatingTimer<LogoutConfirmationDialogView> timer_;

  LogoutButtonTray* owner_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationDialogView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_CONFIRMATION_DIALOG_VIEW_H_
