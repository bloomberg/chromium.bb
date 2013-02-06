// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_
#define ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/views/window/dialog_delegate.h"

namespace aura {
class RootWindow;
}  // namespace aura

namespace gfx {
class Display;
class Size;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ash {
namespace internal {

// Dialog used to show an error messages when unable to change the display
// configuration to mirroring.
class ASH_EXPORT DisplayErrorDialog : public views::DialogDelegateView,
                                      public ash::DisplayController::Observer {
 public:
  // Shows the error dialog.
  static void ShowDialog();

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayErrorDialogTest, Normal);
  FRIEND_TEST_ALL_PREFIXES(DisplayErrorDialogTest, CallTwice);
  FRIEND_TEST_ALL_PREFIXES(DisplayErrorDialogTest, SingleDisplay);
  FRIEND_TEST_ALL_PREFIXES(DisplayErrorDialogTest, DisplayDisconnected);

  DisplayErrorDialog();
  virtual ~DisplayErrorDialog();

  // views::DialogDelegate overrides:
  virtual int GetDialogButtons() const OVERRIDE;

  // views::WidgetDelegate overrides::
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // ash::DisplayController::Observer overrides:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;

  // Returns the pointer of the current instance of this dialog.
  static DisplayErrorDialog* GetInstanceForTest();

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorDialog);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_
