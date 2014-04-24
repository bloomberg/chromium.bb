// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

namespace chromeos {

// Shows a modal warning dialog that the idle action is imminent. Since the
// warning is only really necessary when the idle action is to log out the user,
// the warning is hard-coded to warn about logout.
class IdleActionWarningDialogView : public views::DialogDelegateView {
 public:
  explicit IdleActionWarningDialogView(base::TimeTicks idle_action_time);
  void CloseDialog();

  void Update(base::TimeTicks idle_action_time);

  // views::DialogDelegateView:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;

 private:
  virtual ~IdleActionWarningDialogView();

  void UpdateLabel();

  base::TimeTicks idle_action_time_;

  views::Label* label_;

  base::RepeatingTimer<IdleActionWarningDialogView> update_timer_;

  DISALLOW_COPY_AND_ASSIGN(IdleActionWarningDialogView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
