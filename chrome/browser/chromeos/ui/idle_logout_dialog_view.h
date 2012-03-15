// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
#pragma once

#include "base/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class TimeDelta;
}
namespace views {
class Label;
}

// A class to show the logout on idle dialog if the machine is in retail mode.
class IdleLogoutDialogView : public views::DialogDelegateView {
 public:
  static void ShowDialog();
  static void CloseDialog();

  // views::DialogDelegateView:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  IdleLogoutDialogView();
  virtual ~IdleLogoutDialogView();

  // Calls init after checking if the class is still alive.
  static void CallInit(IdleLogoutDialogView** dialog);
  // Adds the labels and adds them to the layout.
  void Init();

  void Show();
  void Close();

  void UpdateCountdownTimer();

  // Indicate that this instance has been 'closed' and should not be used.
  void set_closed() { *instance_holder_ = NULL; }
  // If our instance holder holds NULL, means we've been closed already.
  bool is_closed() const { return NULL == *instance_holder_; }

  views::Label* restart_label_;
  views::Label* warning_label_;

  // Time at which the countdown is over and we should close the dialog.
  base::Time countdown_end_time_;

  base::RepeatingTimer<IdleLogoutDialogView> timer_;

  // Holds a pointer to our instance; if we are closed, we set this to hold
  // a NULL value, indicating that our instance is been 'closed' and should
  // not be used further. The delete will happen async to the closing.
  IdleLogoutDialogView** instance_holder_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogView);
};

#endif  // CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
