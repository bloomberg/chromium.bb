// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

// Shows a modal warning dialog that the idle action is imminent. Since the
// warning is only really necessary when the idle action is to log out the user,
// the warning is hard-coded to warn about logout.
class IdleActionWarningDialogView : public views::DialogDelegateView {
 public:
  IdleActionWarningDialogView();
  void Close();

  // views::DialogDelegateView:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;

 private:
  virtual ~IdleActionWarningDialogView();

  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(IdleActionWarningDialogView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
