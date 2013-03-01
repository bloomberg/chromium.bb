// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_
#define ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "base/compiler_specific.h"
#include "chromeos/display/output_configurator.h"
#include "ui/views/widget/widget_observer.h"
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
  // Shows the error dialog for |failed_new_state| and returns it.
  static DisplayErrorDialog* ShowDialog(chromeos::OutputState failed_new_state);

  // Update the error message for |failed_new_state|.
  void UpdateMessageForState(chromeos::OutputState failed_new_state);

 private:
  explicit DisplayErrorDialog(chromeos::OutputState failed_new_state);
  virtual ~DisplayErrorDialog();

  // views::DialogDelegate overrides:
  virtual int GetDialogButtons() const OVERRIDE;

  // views::WidgetDelegate overrides::
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // ash::DisplayController::Observer overrides:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorDialog);
};

// The class to observe the output failures and shows the error dialog when
// necessary.
class ASH_EXPORT DisplayErrorObserver
    : public chromeos::OutputConfigurator::Observer,
      public views::WidgetObserver {
 public:
  DisplayErrorObserver();
  virtual ~DisplayErrorObserver();

  const DisplayErrorDialog* dialog() const { return dialog_; }

  // chromeos::OutputConfigurator::Observer overrides:
  virtual void OnDisplayModeChangeFailed(
      chromeos::OutputState failed_new_state) OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

 private:
  DisplayErrorDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorObserver);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ERROR_DIALOG_H_
